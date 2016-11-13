#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

#include "tmtokenmanager.h"

tmTokenManager::tmTokenManager(QObject *parent):baseDevice(parent)
{
    //初始化常数
    tmPort = 10452;
    tmPartnerIndex = 0;//初始化时，指向self

    heartbeatInterv = 1* 1000;      //default = 1sec
    tokenCheckInterv  = 2*1000;     //default = 2sec
    tokenErrorDelay = 5 * 1000;     //default = 5sec

    //初始化对象
    serverSocket = new QUdpSocket(this);
    clientSocket = new QUdpSocket(this);
    connect(serverSocket, SIGNAL(readyRead()),this, SLOT(processPendingDatagrams()));
    clearPeerInfo();//清除列表

    //初始化计时器
    tokenTakeOutOvertimer.setSingleShot(true);
    connect(&tokenTakeOutOvertimer,SIGNAL(timeout()),this,SLOT(tokenTakeOutOvertime()));

    tokenTakeInOvertimer.setSingleShot(true);
    connect(&tokenTakeInOvertimer,SIGNAL(timeout()),this,SLOT(tokenTakeInOvertime()));

    tokenOrderOutOvertimer.setSingleShot(true);
    connect(&tokenOrderOutOvertimer,SIGNAL(timeout()),this,SLOT(tokenOrderOutOvertime()));

    tokenOrderInOvertimer.setSingleShot(true);
    connect(&tokenOrderInOvertimer,SIGNAL(timeout()),this,SLOT(tokenOrderInOvertime()));

    heartBeatTimer.setSingleShot(false);//心跳报文仅当start()调用后开始，可以用stop停止
    heartBeatTimer.setInterval(heartbeatInterv);
    connect(&heartBeatTimer,SIGNAL(timeout()),this,SLOT(heartBeatSender()));

    tokenCheckTimer.setSingleShot(false);//令牌检查器，仅当start()调用后开始，可以用stop停止
    tokenCheckTimer.setInterval(tokenCheckInterv);
    connect(&tokenCheckTimer,SIGNAL(timeout()),this,SLOT(tokenCheck()));

    if( pPeersList.size() ==0 ){
        //需要新建Self
        tmPeer* self = new tmPeer(this);
        pPeersList.append(self);
        connect(self,SIGNAL(msgDeviceChanged(QString)),this,SLOT(peerChanged(QString)));
        self->stopCheckOffline();
    }

    resetAll();
    setState(stateStop);
}

tmTokenManager::~tmTokenManager(){
    clearPeerInfo();
    if(serverSocket){
        serverSocket->deleteLater();
    }
    if(clientSocket){
        clientSocket->deleteLater();
    }
}
int tmTokenManager::save(iLoadSaveProcessor* processor){

    processor->saveParameters( QString("masterPeerMessage") , masterPeerMessage );
    processor->saveParameters( QString("tmPort") , QString::number( tmPort) );
    processor->saveParameters( QString("tmPartnerIndex") , QString::number( tmPartnerIndex) );
    processor->saveParameters( QString("pPeersListSize"), QString::number( pPeersList.size() ));
    baseDevice::save(processor);
    for(int i=0; i<pPeersList.size(); i++){
        processor->createNewInstance( QString("tmPeer"), QString::number(i) );
        processor->moveToInstance( QString("tmPeer"), QString::number(i) );
        pPeersList.at(i)->save(processor);
        processor->moveBackToParent();
    }
    return 0;
}

int tmTokenManager::load(iLoadSaveProcessor* processor){
    int i,number;
    QString value;
    bool ok;
    processor->loadParameters( QString("masterPeerMessage"), &value );
    setMasterPeerMessage(value);
    processor->loadParameters( QString("tmPort") , &value);
    tmPort = value.toInt( &ok, 10);
    processor->loadParameters( QString("tmPartnerIndex") , &value);
    tmPartnerIndex = value.toInt( &ok, 10);
    baseDevice::load(processor);
    processor->loadParameters( QString("pPeersListSize") , &value);
    number = value.toInt( &ok, 10);
    clearPeerInfo();
    for(i= 0; i< number ; i++ ){
        tmPeer* newOne = new tmPeer(this);
        pPeersList.append(newOne);
        connect(newOne,SIGNAL(msgDeviceChanged(QString)),this,SLOT(peerChanged(QString)));
        processor->moveToInstance( QString("tmPeer"), QString::number(i) );
        pPeersList.at(i)->load(processor);
        processor->moveBackToParent();
    }
    return 0;
}


//启动
int tmTokenManager::start(void){

    //qDebug()<<"tmTokenManager::start"<<getSelfPeer()->getPeerIp();
    //1、bind网络
    if( !serverSocket->bind( QHostAddress::AnyIPv4,tmPort,QAbstractSocket::ShareAddress) ) {
        getSelfPeer()->setState( tmPeer::stateDisable );
        setError( errorNetworkError );
        setState( stateStop );
        stop();
        //qDebug()<<"1";
        return -1;
    }
    //2、检查self信息
    if( getSelfPeer()->getName().isEmpty() || getSelfPeer()->getPeerIp()== 0 ){
        getSelfPeer()->setState( tmPeer::stateDisable );
        getSelfPeer()->setError( tmPeer::errorInnerError);
        //qDebug()<<"out";
        setError( errorSelfPeerEmpty );
        setState( stateStop );
        stop();
        return -2;
    }
    //3、检查self状态是否能启动
    if(getSelfPeer()->getError(tmPeer::errorInnerError)){
        getSelfPeer()->setState( tmPeer::stateDisable );
        setError( errorSelfPeerError);
        setState( stateStop );
        stop();
        return -3;
    }
    //4、检查priority和name
    checkPriority();
    checkName();
    //5、自身状态设置为online
    setState( stateRun );
    getSelfPeer()->setState(tmPeer::stateOnlinewithoutToken);
    //6、启动心跳报文
    heartBeatTimer.start();
    //7、启动令牌检查
    tokenCheckTimer.start();
    tokenErrorDelayTime.start();//用于检查elapse

    return 0;
}

//停止
int tmTokenManager::stop(void){
    //接口关闭
    serverSocket->close();
    clientSocket->close();
    //停止心跳
    heartBeatTimer.stop();
    //停止令牌检查
    tokenCheckTimer.stop();
    //自身状态设为Disable
    setState( stateStop );
    getSelfPeer()->setState( tmPeer::stateDisable );
    return 0;
}

//重启
int tmTokenManager::restart(void){
    stop();
    return(start());
}

//检查收到的报文
//此处主要负责分发，不做条件判断
void tmTokenManager::processPendingDatagrams(){
        QByteArray datagram,head,button;
        int paraBegin;
        bool ok;
        QString source,target;
        qint32 overtime;
        QString temp_name;
        quint32 temp_ip, temp_priority;
        quint64 temp_state,temp_error;
        tmPeer newOne;
    while (serverSocket->hasPendingDatagrams()) {
        datagram.resize(serverSocket->pendingDatagramSize());
        if( serverSocket->readDatagram(datagram.data(), datagram.size()) <0) continue;//丢弃该报文
        button = datagram.right(2);
        //qDebug()<<button;
        if( button != "\r\n") continue;//结尾不对，丢弃该报文
        head = datagram.left(3);
        if (head != "$TM") continue;//协议簇不对，丢弃该报文
        head = datagram.mid(3,3);
        if(head == "SHB") {
//      1、从站心跳报文(slave heart beat)
//      $TMSHB,peer名称(max 255 byte),IP地址(4byte),peer状态(1byte),peer错误状态(1byte),peer令牌优先级(1byte)<CR><LF>
            paraBegin = 7;
            temp_name = datagramReadParameter( datagram , &paraBegin);
            temp_ip = datagramReadParameter( datagram , &paraBegin).toUInt(&ok, 16);
            temp_state =  datagramReadParameter( datagram , &paraBegin).toLongLong(&ok, 16);
            temp_error =  datagramReadParameter( datagram , &paraBegin).toLongLong(&ok, 16);
            temp_priority = datagramReadParameter( datagram , &paraBegin).toUInt(&ok,16);
            if( getSelfPeer()->getName() == temp_name
                    && getSelfPeer()->getPeerIp() == temp_ip
                    && getSelfPeer()->getPeerPriority() == temp_priority) continue;//本peer自己发的，丢弃该报文
            //qDebug()<<"tmTokenManager::processPendingDatagrams()"<<temp_name<<temp_error<<temp_ip<<temp_priority;
            newOne.update(temp_name,temp_state,temp_error,temp_priority,temp_ip);
            setPeer(&newOne);
        }
        else if(head == "MHB") {
//      2、主站心跳报文(master heart beat)
//      $TMMHB,peer名称(max 255 byte),IP地址(4byte),peer状态(1byte),peer错误状态(1byte),peer令牌优先级(1byte),状态消息(max 255 byte)<CR><LF>
            paraBegin = 7;
            temp_name = datagramReadParameter( datagram , &paraBegin);

            temp_ip = datagramReadParameter( datagram , &paraBegin).toUInt(&ok, 16);
            temp_state =  datagramReadParameter( datagram , &paraBegin).toLongLong(&ok, 16);
            temp_error =  datagramReadParameter( datagram , &paraBegin).toLongLong(&ok, 16);
            temp_priority = datagramReadParameter( datagram , &paraBegin).toUInt(&ok,16);
            if( getSelfPeer()->getName() == temp_name
                    && getSelfPeer()->getPeerIp() == temp_ip
                    && getSelfPeer()->getPeerPriority() == temp_priority) continue;//本peer自己发的，丢弃该报文
            //qDebug()<<temp_error<<temp_ip<<temp_priority;
            newOne.update(temp_name,temp_state,temp_error,temp_priority,temp_ip);
            setPeer(&newOne);
            //qDebug()<<temp_state<<ret<<pPeersList.size();
            setMasterPeerMessage( QString( datagramReadParameter( datagram , &paraBegin) ) );
        }
        else if(head == "TTR") {
//      3、令牌转移提交(token transfer require)
//      $TMTTR,源peer名称(持有token),目标peer名称,超时时间(秒 4byte)<CR><LF>
            paraBegin = 7;
            source = datagramReadParameter( datagram , &paraBegin);
            target = datagramReadParameter( datagram , &paraBegin);
            overtime = datagramReadParameter( datagram , &paraBegin).toHex().toUInt(&ok,16);
            if (source == getSelfPeer()->getName() ){
                //本peer被动交出令牌
                tmPartnerIndex = getPeerIndex( target );
                tokenOrderOut(overtime);
            }
            else if (target == getSelfPeer()->getName()){
                //本peer被动接受令牌
                tmPartnerIndex = getPeerIndex( source );
                tokenOrderIn(overtime);
            }
        }
        else if(head == "TTA") {
//      4、令牌转移应答确认(token transfer acknowleadge)
//      $TMTTA,源peer名称,目标peer名称<CR><LF>
            paraBegin = 7;
            source = datagramReadParameter( datagram , &paraBegin);
            target = datagramReadParameter( datagram , &paraBegin);
            if (source == getSelfPeer()->getName() ){
                //本peer主动让出令牌，收到确认
                tmPartnerIndex = getPeerIndex( target );
                tokenTakeOutAck();
            }
            if (target == getSelfPeer()->getName()){
                //本peer主动获取令牌，收到确认
                tmPartnerIndex = getPeerIndex( source );
                tokenTakeInAck();
            }
        }
        else if(head == "TTC") {
//      5、令牌转移取消(token transfer cancel)
//      $TMTTC,源peer名称,目标peer名称<CR><LF>
            paraBegin = 7;
            source = datagramReadParameter( datagram , &paraBegin);
            target = datagramReadParameter( datagram , &paraBegin);
            if (getSelfPeer()->getState() == tmPeer::stateTokenTakeOutPending &&
                    source == getSelfPeer()->getName() ){
                //本peer主动让出交出令牌，收到取消
                tmPartnerIndex = getPeerIndex( target );
                tokenTakeOutCancel();
            }
            else if (getSelfPeer()->getState() == tmPeer::stateTokenTakeInPending &&
                     target == getSelfPeer()->getName() ){
                //本peer主动获取令牌，收到取消
                tmPartnerIndex = getPeerIndex( source );
                tokenTakeInCancel();
            }
            else if (getSelfPeer()->getState() == tmPeer::stateTokenOrderInPending &&
                     target == getSelfPeer()->getName() ){
                //本peer被动获取令牌，收到取消
                tmPartnerIndex = getPeerIndex( source );
                tokenOrderInCancel();
            }
            else if (getSelfPeer()->getState() == tmPeer::stateTokenOrderOutPending &&
                     source == getSelfPeer()->getName() ){
                //本peer被动让出令牌，收到取消
                tmPartnerIndex = getPeerIndex( target );
                tokenOrderOutCancel();
            }
        }
        else if(head == "TTF") {
//      6、令牌强制转移(toekn transfer forced)//用途：1.令牌强制获得。2、令牌丢失后，自动生成令牌
//      $TMTTF,目标peer名称<CR><LF>
            paraBegin = 7;
            target = datagramReadParameter( datagram , &paraBegin);
            if (target != getSelfPeer()->getName() ){
                //强制到其他peer
                tmPartnerIndex = getPeerIndex( target );
                tokenForceOrderOut();
            }
        }
        else {
            emit msgOtherCommandReceived( datagram );//无法解析，外送，未来扩展接口
        }
    }
}

//检查token
void tmTokenManager::tokenCheck(){
    int res=findToken();
    //qDebug()<<res<<isSelfFirstPriority()<<tokenErrorDelayTime.elapsed();
    if( res == -1 ){
        //找不到令牌
        if(tokenErrorDelayTime.elapsed() >= tokenErrorDelay){
            setError(errorTokenLost);
            if( isSelfFirstPriority() ){
                //本机有最高优先权则立即获得token
                tokenForceTakeIn();
            }
        }
    }
    else if( res == -2){
        //找到两个或以上令牌
        if(tokenErrorDelayTime.elapsed() >= tokenErrorDelay){
            setError(errorTokenDuplicated);
            if ( getSelfPeer()->isWithToken() && !isSelfFirstPriority() ){
                //本机没有最高优先权则立即取消token
                tokenForceOrderOut();
            }
        }
    }
    else if( res >= 0){
        //找到了token，复位报警
        resetError(errorTokenLost | errorTokenDuplicated);
        tokenErrorDelayTime.start();
    }
}

//发送心跳报文
void tmTokenManager::heartBeatSender(){

    if(getSelfPeer()->getState() == tmPeer::stateDisable) return;

    //qDebug()<<"heartBeat";

    if(getSelfPeer()->isWithToken()){
//      master
//      2、主站心跳报文(master heart beat)
//      $TMMHB,peer名称(max 255 byte),IP地址(4byte),peer状态(1byte),peer错误状态(1byte),peer令牌优先级(1byte),状态消息(max 255 byte)<CR><LF>
//      注意：消息的具体内容由其他程序打包送入和取出解析。
        QByteArray heartBeat;
        heartBeat = "$TMMHB,";
        heartBeat += getSelfPeer()->getName().toUtf8();
        heartBeat += ",";
        heartBeat += QByteArray::number(getSelfPeer()->getPeerIp(),16);
        heartBeat += ",";
        heartBeat += QByteArray::number(getSelfPeer()->getState(),16);
        heartBeat += ",";
        heartBeat += QByteArray::number(getSelfPeer()->getError(),16);
        heartBeat += ",";
        heartBeat += QByteArray::number(getSelfPeer()->getPeerPriority(),16);
        heartBeat += ",";
        heartBeat +=  masterPeerMessage;
        heartBeat += 0x0D;
        heartBeat += 0x0A;
        //qDebug()<<heartBeat;
        clientSocket->writeDatagram(heartBeat.data(), heartBeat.size(),QHostAddress::Broadcast, tmPort);
    }
    else{
//      slave
//      1、从站心跳报文(slave heart beat)
//      $TMSHB,peer名称(max 255 byte),IP地址(4byte),peer状态(1byte),peer错误状态(1byte),peer令牌优先级(1byte)<CR><LF>

        QByteArray heartBeat;
        heartBeat = "$TMSHB,";
        heartBeat += getSelfPeer()->getName().toUtf8();//.toLocal8Bit();
        heartBeat += ",";
        heartBeat += QByteArray::number(getSelfPeer()->getPeerIp(),16);
        heartBeat += ",";
        heartBeat += QByteArray::number(getSelfPeer()->getState(),16);
        heartBeat += ",";
        heartBeat += QByteArray::number(getSelfPeer()->getError(),16);
        heartBeat += ",";
        heartBeat += QByteArray::number(getSelfPeer()->getPeerPriority(),16);
        heartBeat += 0x0D;
        heartBeat += 0x0A;
        //qDebug()<<heartBeat;
        clientSocket->writeDatagram(heartBeat.data(), heartBeat.size(),QHostAddress::Broadcast, tmPort);
    }
}


int tmTokenManager::clearPeerInfo(){
    for (int i=0; i<pPeersList.size();i++){
        disconnect(pPeersList.at(i),SIGNAL(msgDeviceChanged(QString)),this,SLOT(peerChanged(QString)) );
        delete pPeersList.at(i);
    }//释放内存
    pPeersList.clear();//清空QList
    return 0;
}


/////////////////////////////////1、本机主动交出令牌
//本机主动交出令牌 请求
//2016.10修改：
//为简化操作流程，index可以不指定（默认为-1）。此时自动获取唯一没有token的peer的index，若是多于一个peer没有token则 整个函数返回-1.
int tmTokenManager::tokenTakeOut(int index, qint32 overtime){
    if(getState() != stateRun) return -6;//tokenManager不在运行状态
    if(index == -1){
        //查找唯一没有token的peer
        int count =0;
        for(int i=1; i<pPeersList.size();i++ ){
            if(getPeer(i)->getState() == tmPeer::stateOnlinewithoutToken){
                count++;
                index = i;
            }
        }
        if(count == 1){
            tmPartnerIndex = index;
        }
        else return -1;//找不到partner
    }
    else{
        if(!isIndexValid(index) || index == 0 ) return -1;//找不到partner
        tmPartnerIndex = index;
    }
    if(overtime <0) return -1;   //overTime数据不对
    if(getSelfPeer()->getState() != tmPeer::stateOnlinewithToken ) return -2;//仅当本peer在线有token时才能进入takeout
    if(getSelfPeer()->getError() == tmPeer::errorInnerError || getError() == errorNetworkError) return -3;//本peer未准备好
    if(getPartner()->getState() != tmPeer::stateOnlinewithoutToken ) return -4;//仅当目标peer在线无token时才能进入takeout
    if(getPartner()->getError() == tmPeer::errorInnerError || getError() == errorNetworkError) return -5;//目标peer没有准备好
    getSelfPeer()->setState( tmPeer::stateTokenTakeOutPending );
    if(overtime>0){
        tokenTakeOutOvertimer.setInterval(overtime);
        tokenTakeOutOvertimer.start();
    }
//3、令牌转移提交(token transfer require)
//$TMTTR,源peer名称(持有token),目标peer名称,超时时间(秒 4byte)<CR><LF>
    QByteArray require;
    require = "$TMTTR,";
    require += getSelfPeer()->getName();
    require += ",";
    require += getPartner()->getName();
    require += ",";
    require += overtime>>24;
    require += overtime>>16;
    require += overtime>>8;
    require += overtime;
    require += 0x0D;
    require += 0x0A;
    qint64 res=clientSocket->writeDatagram(require.data(), require.size(),QHostAddress::Broadcast, tmPort);
    //qDebug()<<require<<overtime;
    return res;
}
int tmTokenManager::tokenTakeOut(QString target, qint32 overtime){
    int index=getPeerIndex(target);
    if(index <=0 || index >= pPeersList.size() ) return -1;//找不到目标对应的数据
    return tokenTakeOut(index,overtime);
}
//本机主动交出令牌 超时
void tmTokenManager::tokenTakeOutOvertime(){
    if( getSelfPeer()->getState() == tmPeer::stateTokenTakeOutPending ){
        tokenTakeOutCancel();
        getSelfPeer()->setError(tmPeer::errorTakeOutOverTime);
    }
}
//本机主动交出令牌 确认
int tmTokenManager::tokenTakeOutAck(){
    if(getSelfPeer()->getState() ==  tmPeer::stateTokenTakeOutPending ){
        getSelfPeer()->setState( tmPeer::stateOnlinewithoutToken );
        return 0;
    }
    else return -1;
}
//本机主动交出令牌 取消
int tmTokenManager::tokenTakeOutCancel(){
    if(getSelfPeer()->getState() == tmPeer::stateTokenTakeOutPending ){
//      5、令牌转移取消(token transfer cancel)
//      $TMTTC,源peer名称,目标peer名称<CR><LF>
        QByteArray requrie;
        requrie = "$TMTTC,";
        requrie += getSelfPeer()->getName();
        requrie += ",";
        requrie += getPartner()->getName();
        requrie += 0x0D;
        requrie += 0x0A;
        //qDebug()<<"tokenTakeOutCancel:"<<requrie;
        qint64 res=clientSocket->writeDatagram(requrie.data(), requrie.size(),QHostAddress::Broadcast, tmPort);
        getSelfPeer()->setState( tmPeer::stateOnlinewithToken );
        return res;
    }
    else return -1;
}
/////////////////////////////////2、本机主动获得令牌
//本机主动获得令牌 请求
//2016.10修改：
//为简化操作流程，index可以不指定（默认为-1）。此时自动获取唯一有token的peer的index，若是本机有token或没有peer有token则返回-1.
int tmTokenManager::tokenTakeIn(int index, qint32 overtime){
    if(getState() != stateRun) return -6;//tokenManager不在运行状态
    if(index == -1){
        //查找唯一有token的peer
        int count =0;
        for(int i=1; i<pPeersList.size();i++ ){
            if(getPeer(i)->getState() == tmPeer::stateOnlinewithToken){
                count++;
                index = i;
            }
        }
        if(count == 1){
            tmPartnerIndex = index;
        }
        else return -1;//找不到partner
    }
    else{
        if(!isIndexValid(index) || index == 0 ) return -1;//找不到partner
        tmPartnerIndex = index;
    }
    if(overtime <0) return -1;   //overTime数据不对
    if(getSelfPeer()->getState() != tmPeer::stateOnlinewithoutToken ) return -2;//仅当本peer在线无token时才能进入takeout
    if(getSelfPeer()->getError() == tmPeer::errorInnerError || getError() == errorNetworkError) return -3;//本peer未准备好
    if(getPartner()->getState() != tmPeer::stateOnlinewithToken ) return -4;//仅当目标peer在线有token时才能进入takeout
    if(getPartner()->getError() == tmPeer::errorInnerError || getError() == errorNetworkError) return -5;//目标peer没有准备好

    getSelfPeer()->setState( tmPeer::stateTokenTakeInPending );
    if(overtime>0){
        tokenTakeInOvertimer.setInterval(overtime);
        tokenTakeInOvertimer.start();
    }

//      3、令牌转移提交(token transfer require)
//      $TMTTR,源peer名称(持有token),目标peer名称,超时时间(秒 4byte)<CR><LF>
    QByteArray require;
    require = "$TMTTR,";
    require += getPartner()->getName();
    require += ",";
    require += getSelfPeer()->getName();
    require += ",";
    require += overtime>>24;
    require += overtime>>16;
    require += overtime>>8;
    require += overtime;
    require += 0x0D;
    require += 0x0A;
    //qDebug()<<requrie;
    qint64 res=clientSocket->writeDatagram(require.data(), require.size(),QHostAddress::Broadcast, tmPort);
    return res;
}
int tmTokenManager::tokenTakeIn(QString target,qint32 overtime){
    int index=getPeerIndex(target);
    if(index <=0 || index >= pPeersList.size() ) return -1;
    return tokenTakeIn(index,overtime);
}
//本机主动获得令牌 超时
void tmTokenManager::tokenTakeInOvertime(){
    if(getSelfPeer()->getState() == tmPeer::stateTokenTakeInPending ){
        tokenTakeInCancel();
        getSelfPeer()->setError( tmPeer::errorTakeInOverTime );
    }
}
//本机主动获得令牌 确认
int tmTokenManager::tokenTakeInAck(){
    if(getSelfPeer()->getState() == tmPeer::stateTokenTakeInPending ){
        getSelfPeer()->setState( tmPeer::stateOnlinewithToken );
        return 0;
    }
    else return -1;
}
//本机主动获得令牌 取消
int tmTokenManager::tokenTakeInCancel(){
    if(getSelfPeer()->getState() == tmPeer::stateTokenTakeInPending ){
//      5、令牌转移取消(token transfer cancel)
//      $TMTTC,源peer名称,目标peer名称<CR><LF>
        QByteArray requrie;
        requrie = "$TMTTC,";
        requrie += getPartner()->getName();
        requrie += ",";
        requrie += getSelfPeer()->getName();
        requrie += 0x0D;
        requrie += 0x0A;
        qint64 res=clientSocket->writeDatagram(requrie.data(), requrie.size(),QHostAddress::Broadcast, tmPort);
        //qDebug()<<"tokenTakeInCancel:"<<requrie;
        getSelfPeer()->setState( tmPeer::stateOnlinewithoutToken );
        return res;
    }
    else return -1;
}

/////////////////////////////////3、本机被动交出令牌
//本机被动将令牌交出 请求
int tmTokenManager::tokenOrderOut(qint32 overtime){
    if(getState() != stateRun) return -6;//tokenManager不在运行状态
    if(tmPartnerIndex <=0 || tmPartnerIndex >= pPeersList.size() ) return -1;//找不到目标对应的数据
    if(overtime <0) return -1;   //overTime数据不对
    if(getSelfPeer()->getState() != tmPeer::stateOnlinewithToken ) return -2;//仅当本peer在线有无token时才能进入takeout
    if(getSelfPeer()->getError() == tmPeer::errorInnerError || getError() == errorNetworkError) return -3;//本peer未准备好
    if(getPartner()->getState() != tmPeer::stateOnlinewithoutToken ) return -4;//仅当目标peer在线无token时才能进入takeout
    if(getPartner()->getError() == tmPeer::errorInnerError || getError() == errorNetworkError) return -5;//目标peer没有准备好

    getSelfPeer()->setState( tmPeer::stateTokenOrderOutPending );

    if(overtime>0){
        tokenOrderOutOvertimer.setInterval(overtime);
        tokenOrderOutOvertimer.start();
    }
    return 0;

}
//本机被动交出令牌 超时
void tmTokenManager::tokenOrderOutOvertime(){
    if(getSelfPeer()->getState() == tmPeer::stateTokenTakeInPending ){
        getSelfPeer()->setError( tmPeer::errorOrderOutOverTime );
        getSelfPeer()->setState( tmPeer::stateOnlinewithToken );
        //tokenOrderOutCancel();
    }
}
//本机被动交出令牌 确认
int tmTokenManager::tokenOrderOutAck(){
    if(getSelfPeer()->getState() == tmPeer::stateTokenOrderOutPending ){
        getSelfPeer()->setState( tmPeer::stateOnlinewithoutToken );
//      4、令牌转移应答确认(token transfer acknowleadge)
//      $TMTTA,源peer名称,目标peer名称<CR><LF>
        QByteArray ack;
        ack = "$TMTTA,";
        ack += getSelfPeer()->getName();
        ack += ",";
        ack += getPartner()->getName();
        ack += 0x0D;
        ack += 0x0A;
        qint64 res=clientSocket->writeDatagram(ack.data(), ack.size(),QHostAddress::Broadcast, tmPort);
        return res;
    }
    else return -1;
}
//本机被动交出令牌 取消
int tmTokenManager::tokenOrderOutCancel(){
    if(getSelfPeer()->getState() == tmPeer::stateTokenOrderOutPending ){
//      5、令牌转移取消(token transfer cancel)
//      $TMTTC,源peer名称,目标peer名称<CR><LF>
        QByteArray requrie;
        requrie = "$TMTTC,";
        requrie += getSelfPeer()->getName();
        requrie += ",";
        requrie += getPartner()->getName();
        requrie += 0x0D;
        requrie += 0x0A;
        qint64 res=clientSocket->writeDatagram(requrie.data(), requrie.size(),QHostAddress::Broadcast, tmPort);
        getSelfPeer()->setState( tmPeer::stateOnlinewithToken );
        return res;
    }
    else return -1;
}

/////////////////////////////////4、本机被动获得令牌
//本机被动获得令牌 请求
int tmTokenManager::tokenOrderIn( qint32 overtime){
    if(getState() != stateRun) return -6;//tokenManager不在运行状态
    if(tmPartnerIndex <=0 || tmPartnerIndex >= pPeersList.size() ) return -1;//找不到目标对应的数据
    if(overtime <0) return -1;   //overTime数据不对
    if(getSelfPeer()->getState() != tmPeer::stateOnlinewithoutToken ) return -2;//仅当本peer在线无token时才能进入takeout
    if(getSelfPeer()->getError() == tmPeer::errorInnerError || getError() == errorNetworkError) return -3;//本peer未准备好
    if(getPartner()->getState() != tmPeer::stateOnlinewithToken ) return -4;//仅当目标peer在线有token时才能进入takeout
    if(getPartner()->getError() == tmPeer::errorInnerError || getError() == errorNetworkError) return -5;//目标peer没有准备好

    getSelfPeer()->setState( tmPeer::stateTokenOrderInPending );

    if(overtime>0){
        tokenOrderInOvertimer.setInterval(overtime);
        tokenOrderInOvertimer.start();
    }
    //qDebug()<<"tokenOrderIn"<<overtime<<tokenOrderInOvertimer.isActive();
    return 0;
}
//本机被动获得令牌 超时
void tmTokenManager::tokenOrderInOvertime(){
    if(getSelfPeer()->getState() == tmPeer::stateTokenOrderInPending ){
        getSelfPeer()->setError( tmPeer::errorOrderInOverTime );
        getSelfPeer()->setState( tmPeer::stateOnlinewithoutToken );
        //tokenOrderInCancel();
        //qDebug()<<"tokenOrderInOvertime";
    }
}
//本机被动获得令牌 确认
int tmTokenManager::tokenOrderInAck(){
    if(getSelfPeer()->getState() == tmPeer::stateTokenOrderInPending ){
        getSelfPeer()->setState( tmPeer::stateOnlinewithToken );
//      4、令牌转移应答确认(token transfer acknowleadge)
//      $TMTTA,源peer名称,目标peer名称<CR><LF>
        QByteArray ack;
        ack = "$TMTTA,";
        ack += getPartner()->getName();
        ack += ",";
        ack += getSelfPeer()->getName();
        ack += 0x0D;
        ack += 0x0A;
        qint64 res=clientSocket->writeDatagram(ack.data(), ack.size(),QHostAddress::Broadcast, tmPort);
        return res;
    }
    else return -1;
}
//本机被动获得令牌 取消
int tmTokenManager::tokenOrderInCancel(){
    if(getSelfPeer()->getState() == tmPeer::stateTokenOrderInPending ){
//      5、令牌转移取消(token transfer cancel)
//      $TMTTC,源peer名称,目标peer名称<CR><LF>
        QByteArray requrie;
        requrie = "$TMTTC,";
        requrie += getPartner()->getName();
        requrie += ",";
        requrie += getSelfPeer()->getName();
        requrie += 0x0D;
        requrie += 0x0A;
        qint64 res=clientSocket->writeDatagram(requrie.data(), requrie.size(),QHostAddress::Broadcast, tmPort);
        getSelfPeer()->setState( tmPeer::stateOnlinewithoutToken );
        return res;
    }
    else return -1;
}

//强制切出
int tmTokenManager::tokenForceOrderOut(){
    if(getSelfPeer()->getState() != tmPeer::stateDisable){
        getSelfPeer()->setState( tmPeer::stateOnlinewithoutToken );
        return 0;
    }
    else return -1;
}
//强制切入
int tmTokenManager::tokenForceTakeIn(){
    if(getState() != stateRun) return -6;//tokenManager不在运行状态
    if(getSelfPeer()->getState() == tmPeer::stateDisable || getSelfPeer()->getError( tmPeer::errorInnerError ) || getError( errorNetworkError)) return -1;//本peer未准备好
//      6、令牌强制转移(toekn transfer forced)//用途：1.令牌强制获得。2、令牌丢失后，自动生成令牌
//      $TMTTF,目标peer名称<CR><LF>
    int index=findToken();
    if(index ==-2 || index ==0 ){
        //网上token过多or本peer有token 则返回-2
        return -2;
    }
    if (index >0){
        if( pPeersList.at(index)->getPeerPriority() > getSelfPeer()->getPeerPriority() ){
            //若现在的主站优先级比本peer高，则返回-3
            //qDebug()<<"here";
            return -3;
        }
    }
    QByteArray requrie;
    requrie = "$TMTTF,";
    requrie += getSelfPeer()->getName();
    requrie += 0x0D;
    requrie += 0x0A;
    qint64 res=clientSocket->writeDatagram(requrie.data(), requrie.size(),QHostAddress::Broadcast, tmPort);
    getSelfPeer()->setState( tmPeer::stateOnlinewithToken );
    return res;
}


//返回本机状态
bool tmTokenManager::isStarted(){
    if(getState() == stateRun) return true;
    else return false;
}
//判断index是否合法
bool tmTokenManager::isIndexValid(int index){
    if(index <0 || index >=pPeersList.size() ){
        return false;
    }
    return true;
}

//判断本peer是否最高优先级。
bool tmTokenManager::isSelfFirstPriority(){
    quint32 self= getSelfPeer()->getPeerPriority();
    int index=1;
    while(index<pPeersList.size() ){
        if(  pPeersList.at(index)->getState() != tmPeer::stateDisable &&
             pPeersList.at(index)->getState() != tmPeer::stateOffline &&
                self < pPeersList.at(index)->getPeerPriority()    )
            return false;
        index++;
    }

    return true;
}

//检查优先等级，发现相同优先等级且本peer的ip地址最后一段较小，则本peer的优先级++
void tmTokenManager::checkPriority(){
    quint32 self = getSelfPeer()->getPeerPriority();
    quint32 aux1 = getSelfPeer()->getPeerIp();
    quint32 aux2 = (qint8)getSelfPeer()->getName().right(1).toLocal8Bit().toInt();
    //qDebug()<<"checkPriority";
    int index = 1;
    while(index<pPeersList.size()){
        if(self == getPeer(index)->getPeerPriority()
                && aux1 < (qint8)getPeer(index)->getPeerIp() ){
            self++;
            getSelfPeer()->setPeerPriority(self);
        }
        if(self == getPeer(index)->getPeerPriority()
                && aux2 < (qint8)getPeer(index)->getName().right(1).toLocal8Bit().toInt()){
            self++;
            getSelfPeer()->setPeerPriority(self);
        }
        index++;
    }
}

//检查名字，发现相同名字且本peer优先级较小，则本peer的名字+随机码
//2016.10修改：发现相同名字，且优先级相同，则ip地址比较小的peer的名字+随机码
void tmTokenManager::checkName(){
    QString self = getSelfPeer()->getName();
    quint32 aux  = getSelfPeer()->getPeerPriority();
    quint32 aux1 = getSelfPeer()->getPeerIp();
    QTime time = QTime::currentTime();
    qsrand(time.msec());
    int index = 1;
    while(index<pPeersList.size()){
        if(self == getPeer(index)->getName() ){
            if(aux < getPeer(index)->getPeerPriority() ||
                    ( aux == getPeer(index)->getPeerPriority() && aux1 < getPeer(index)->getPeerIp() ) ){
                self += QString::number(qrand());
                getSelfPeer()->setName( self );
            }
        }
        index++;
    }
}

//查找报文中以逗号分割的参数
QByteArray tmTokenManager::datagramReadParameter(const QByteArray & data, int *begin){
    QByteArray comma(",");
    QByteArray end;
    end.append(0x0D).append(0x0A);
    int len = data.indexOf(comma,*begin) - *begin;  //报文指定起始点开始找第一个comma，算长度

    QByteArray parameter;
    parameter.resize(len);
    parameter= data.mid(*begin,len);
    parameter.replace(end,QByteArray(""));
    *begin += (len +1);  //begine 迭代
    return parameter;
}

//找到令牌所在peer的index，找不到则返回-1，多于1个则返回-2，同时修改报警状态
int tmTokenManager::findToken(){
    int count=0;
    int ret =-1;
    int index=0;
    while(index<pPeersList.size()){
        if(pPeersList.at(index)->isWithToken()){
            count++;
            //qDebug()<<count;
            ret = index;
        }
        index++;
    }
    if(count ==0) return -1;
    if(count ==1) return ret;
    if(count >1) return -2;
}

//set & get
QString tmTokenManager::getMasterPeerMessage() const{
    return masterPeerMessage;
}
void tmTokenManager::setMasterPeerMessage(QString& msg){
    if(masterPeerMessage == msg) return; //无改变
    masterPeerMessage = msg;
    emit msgMasterPeerMessageUpdated();
}

int tmTokenManager::setSelfPeer( tmPeer* newone){
    if( pPeersList.size() ==0 ){
        //需要新建Self
        tmPeer* self = new tmPeer(this);
        pPeersList.append(self);
        connect(self,SIGNAL(msgDeviceChanged(QString)),this,SLOT(peerChanged(QString)));
        self->stopCheckOffline();
    }
    //qDebug()<<newone->getName()<<newone->getState()<<newone->getError()<<newone->getPeerPriority()<<newone->getPeerIp();
    setState(stateStop);
    getSelfPeer()->update(newone->getName(),newone->getState(),newone->getError(),newone->getPeerPriority(),newone->getPeerIp() );
    return 0;
}

inline tmPeer* tmTokenManager::getSelfPeer() const{
    return pPeersList.at(0);
}

tmPeer* tmTokenManager::getPartner() const{
    return pPeersList.at(tmPartnerIndex);
}


int tmTokenManager::setPeer(tmPeer *newone){
    if( pPeersList.size() ==0 ){
        //需要新建Self
        tmPeer* self = new tmPeer(this);
        pPeersList.append(self);
        connect(self,SIGNAL(msgDeviceChanged(QString)),this,SLOT(peerChanged(QString)));
        self->stopCheckOffline();
    }
    //qDebug()<<newone->getName()<<newone->getPeerIp()<<newone->getPeerPriority();
    if(newone->getName() != "" && newone->getPeerIp() != 0x0){
        tmPeer* it =0;
        int i;
        for (i=1; i<pPeersList.size();i++){
            it = pPeersList.at(i);
            if( it->getName() == newone->getName()){
                if( it->getState() == newone->getState() &&
                        it->getError()  ==  newone->getError() &&
                        it->getPeerIp() ==  newone->getPeerIp() &&
                        it->getPeerPriority() ==  newone->getPeerPriority() ) {
                    //完全相同，只要更新时间
                    //qDebug()<<"tmTokenManager::setPeer just update";
                    it->update();
                    return -2;
                }
                //qDebug()<<"tmTokenManager::setPeer update all";
                it->update(newone->getName(),newone->getState(),newone->getError(),newone->getPeerPriority(),newone->getPeerIp());//发现同名节点，替换
                checkPriority();
                checkName();
                return 0;
            }
        }
        it = new tmPeer(this);
        pPeersList.append(it);
        connect(it,SIGNAL(msgDeviceChanged(QString)),this,SLOT(peerChanged(QString)));
        it->update(newone->getName(),newone->getState(),newone->getError(),newone->getPeerPriority(),newone->getPeerIp());//未发现同名节点，新建
        checkPriority();
        checkName();
        return 0;
    }
    else return -1;
}

int tmTokenManager::generateSelfInfo(){
    if( pPeersList.size() ==0 ){
        //需要新建Self
        tmPeer* self = new tmPeer(this);
        pPeersList.append(self);
        connect(self,SIGNAL(msgDeviceChanged(QString)),this,SLOT(peerChanged(QString)));
        self->stopCheckOffline();
    }
    QTime time = QTime::currentTime();
    qsrand(time.msec());
    QString temp_name = QString::fromLocal8Bit("default_station_%1").arg(qrand());
    quint32 temp_ip;
    foreach( QHostAddress t, QNetworkInterface::allAddresses()){
        if (t.protocol() == QAbstractSocket::IPv4Protocol && !t.isLoopback()){
            temp_ip = t.toIPv4Address();
            //qDebug()<<t;
        }
    }
    getSelfPeer()->update(temp_name,tmPeer::stateOnlinewithoutToken,0,0,temp_ip);
    return 0;
}

//根据名称找tmPeer*
tmPeer* tmTokenManager::getPeer(QString& name) const{
    for (int i=1; i<pPeersList.size();i++){
        if( pPeersList.at(i)->getName() == name){
            return pPeersList.at(i);
        }
    }
    return nullptr;
}
//根据序号找tmPeer*
tmPeer* tmTokenManager::getPeer(int index) const{
    if( index <0 || index >=pPeersList.size() )
        return nullptr;
    return pPeersList.at(index);
}
//根据名称查找index
int tmTokenManager::getPeerIndex(QString& name) const{
    for (int i=0; i<pPeersList.size();i++){
        if( pPeersList.at(i)->getName() == name){
            return i;
        }
    }
    return -1;
}

void tmTokenManager::peerChanged(QString name){
    int index = getPeerIndex( name );
    //qDebug()<<"tmTokenManager::peerChanged"<<index<<name;
    if(index < 0 ) return;
    emit msgPeersListChanged(index);
}
