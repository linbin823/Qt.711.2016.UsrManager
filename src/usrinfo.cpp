#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

#include "tmpeer.h"

tmPeer::tmPeer(QObject *parent) : baseDevice(parent)
{
    QTime time = QTime::currentTime();
    qsrand(time.msec());
    setName( QString("default peer name %1").arg(qrand()) );
    peerIp = 0;
    priority = 0;
    lastUpdateTime.start();
    resetError(0xFFFFFFFFFFFFFFFF);
    setState(stateDisable);

    offlineDelay = 30 * 1000;       //default = 30sec
    offlineCheckInterv = 1* 1000;   //default = 1sec

    offlineCheckTimer.setSingleShot(false);
    offlineCheckTimer.setInterval(offlineCheckInterv);
    connect(&offlineCheckTimer,SIGNAL(timeout()),this,SLOT(offlineCheck()));
    offlineCheckTimer.start();//一直都会检查超时

}


void tmPeer::setPeerPriority(quint32 pri){
    if(pri<=0 ) return;
    if(priority != pri){
        priority = pri;
        emit msgPriorityChanged(pri);
        emit msgDeviceChanged(getName());
        //qDebug()<<"newPriority"<<peerName<<pri;
    }
}

void tmPeer::setPeerIp(quint32 ip){
    if(ip<=0 ) return;
    if(peerIp != ip){
        peerIp = ip;
        emit msgIpChanged(ip);
        emit msgDeviceChanged(getName());
        //qDebug()<<"newIp"<<peerName<<ip;
    }
}

quint32 tmPeer::getPeerPriority() const{
    return priority;
}
quint32 tmPeer::getPeerIp() const{
    return peerIp;
}

void tmPeer::update(const QString& name, quint64 state, quint64 error, quint32 pri, quint32 ip){
    //qDebug()<<name<<state<<error<<pri<<ip;
    setName(name);
    setState(state);
    setPeerPriority(pri);
    setPeerIp(ip);
    updateError(error);
    lastUpdateTime.start();
}


void tmPeer::update(){
    lastUpdateTime.start();
}

//检查站offline
void tmPeer::offlineCheck(){
    if( lastUpdateTime.elapsed() >= offlineDelay ){
        //set offline
        if(getState() != stateOffline){
            //qDebug()<<"setOffline";
            setState(stateOffline);
        }
    }
    //will automatically change the state to Online aft heartbeat received
}

//判断是否有令牌，有=true
//disable                   = 0x0000000000000001ULL,      //peer禁用
//onlinewithToken           = 0x0000000000000002ULL,      //peer在线有令牌，主站
//onlinewithoutToken        = 0x0000000000000004ULL,      //peer在线无令牌，从站
//tokenTakeOutPending       = 0x0000000000000008ULL,      //该peer主动把令牌给其他peer，但其他peer还未确认。该peer具有令牌，主站。
//tokenOrderOutPending      = 0x0000000000000010ULL,      //其他peer要求该peer的令牌，该peer还未确认。该peer具有令牌，主站。
//tokenTakeInPending        = 0x0000000000000020ULL,      //该peer主动要获得令牌，但令牌持有peer还未确认。该peer无令牌，从站。
//tokenOrderInPending       = 0x0000000000000040ULL,      //令牌持有peer要求把令牌传送给该peer，但该peer还未确认。该peer无令牌，从站。
//offline                   = 0x0000000000000080ULL,      //peer离线。此状态不适用于本站，用于判断其他peer的掉线状态。
bool tmPeer::isWithToken(){
    if ( getState() == stateOnlinewithToken ||
            getState() == stateTokenTakeOutPending ||
                getState() == stateTokenOrderOutPending   )
        return true;
    else return false;
}
bool tmPeer::stopCheckOffline(){
    offlineCheckTimer.stop();
    return false;
}

int tmPeer::save(iLoadSaveProcessor* processor){

    processor->saveParameters( QString("peerIp"), QString::number(peerIp) );
    processor->saveParameters( QString("priority"), QString::number(priority) );

    processor->saveParameters( QString("offlineDelay"), QString::number(offlineDelay) );
    processor->saveParameters( QString("offlineCheckInterv"), QString::number(offlineCheckInterv) );

    baseDevice::save(processor);
    return 0;
}

int tmPeer::load(iLoadSaveProcessor* processor){
    QString value;
    bool ok;
    processor->loadParameters( QString("peerIp"), &value );
    peerIp = value.toUInt(&ok, 10);
    //qDebug()<<"tmPeer::load"<<ok;
    processor->loadParameters( QString("priority"), &value );
    priority = value.toInt(&ok, 10);
    processor->loadParameters( QString("offlineDelay"), &value );
    offlineDelay = value.toInt(&ok, 10);
    processor->loadParameters( QString("offlineCheckInterv"), &value );
    offlineCheckInterv = value.toInt(&ok, 10);

    baseDevice::load(processor);
    return 0;
}
