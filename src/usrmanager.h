#ifndef USRMANAGER_H
#define USRMANAGER_H

#include <QObject>
#include <QList>
#include <QUdpSocket>
#include <QTime>
#include <QTimer>
#include <QNetworkInterface>
#include "basedevice.h"
#include "tmpeer.h"
#include "iloadsaveprocessor.h"


class tmTokenManager  : public baseDevice
{
    Q_OBJECT
public:
    explicit tmTokenManager(QObject *parent = 0);
    ~tmTokenManager();

    //状态机错误状态
    enum state: quint64{
        stateStop                        = 0x0000000000000001ULL,      //令牌管理器停止
        stateRun                         = 0x0000000000000002ULL,      //令牌管理器运行
    };

    enum errorType : quint64 {
        errorTokenLost                   = 0x0000000000000001ULL,      //网络上没有令牌
        errorTokenDuplicated             = 0x0000000000000002ULL,      //网络上有重令牌
        errorPeerNameDuplicated          = 0x0000000000000004ULL,      //网络上有重名
        errorPeerPriorityDuplicated      = 0x0000000000000008ULL,      //网络上有重优先级
        errorNetworkError                = 0x0000000000000010ULL,      //网络故障，致命必须重启
        errorSelfPeerEmpty               = 0x0000000000000020ULL,      //信息不全（本peer为空），无法启动
        errorSelfPeerError               = 0x0000000000000040ULL,      //本peer错误，无法启动
    };

    virtual QString getStateString(int lang =0 ) const{
        if(lang == langEN){
            switch(getState() ){
                case stateStop:
                    return QString("token manager stop");
                case stateRun:
                    return QString("token manager start");
                default:
                    return QString("error code not found");
            }
        }
        else if(lang == langCHN){
            switch(getState() ){
                case stateStop:
                    return tr("令牌管理器停止");
                case stateRun:
                    return tr("令牌管理器运行");
                default:
                    return tr("找不到错误代码");
            }
        }
    }

    virtual QString getErrorString(quint64 errorCode, int lang =0 ) const{
        if(lang == langEN){
            switch(errorCode){
                case errorTokenLost:
                    return QString("token not found in the LAN");
                case errorTokenDuplicated:
                    return QString("token duplicated in the LAN");
                case errorPeerNameDuplicated:
                    return QString("name duplicated in the LAN");
                case errorPeerPriorityDuplicated:
                    return QString("priority duplicated in the LAN");
                case errorNetworkError:
                    return QString("token manager fatal error, restart the app!");
                case errorSelfPeerEmpty:
                    return QString("incompleted self peer info, unable to start");
                case errorSelfPeerError:
                    return QString("self peer fault, unable to start");
                default:
                    return QString("error code not found");
            }
        }
        else if(lang == langCHN){
            switch(errorCode){
                case errorTokenLost:
                    return tr("网络上没有令牌");
                case errorTokenDuplicated:
                    return tr("网络上有重令牌");
                case errorPeerNameDuplicated:
                    return tr("网络上有重名");
                case errorPeerPriorityDuplicated:
                    return tr("网络上有重优先级");
                case errorNetworkError:
                    return tr("令牌管理器网络故障，致命必须重启");
                case errorSelfPeerEmpty:
                    return tr("信息不全（本peer为空），无法启动");
                case errorSelfPeerError:
                    return tr("本peer错误，无法启动");
                default:
                    return tr("找不到错误代码");
            }
        }

    }

    virtual int save(iLoadSaveProcessor* processor);
    virtual int load(iLoadSaveProcessor* processor);


//others
private:

    QList<tmPeer*> pPeersList;
    QString masterPeerMessage;
    QUdpSocket* clientSocket;
    QUdpSocket* serverSocket;
    QTimer tokenTakeOutOvertimer, tokenTakeInOvertimer, tokenOrderOutOvertimer, tokenOrderInOvertimer;
    QTimer heartBeatTimer,tokenCheckTimer;
    QTime tokenErrorDelayTime;

    int heartbeatInterv;
    int tokenErrorDelay;
    int tokenCheckInterv;
    int tmPort;
    int tmPartnerIndex;//交互方的index

    QByteArray datagramReadParameter(const QByteArray & data, int *begin); //读取报文中以逗号分割的参数
    int findToken();                    //找到令牌所在peer的index，找不到则返回-1，多于1个则返回-2，同时修改报警状态
    void checkPriority();               //检查优先等级，发现相同优先等级且本peer的ip地址最后一段较小，则本peer的优先级++
    void checkName();                   //检查名字，发现相同名字且本peer优先级较小，则本peer的名字+随机码

private slots:
    void processPendingDatagrams();
    void heartBeatSender();
    void tokenCheck();
    void peerChanged(QString name);

public:
    //token take out
    Q_INVOKABLE int tokenTakeOut(QString target, qint32 overtime=10000);
    Q_INVOKABLE int tokenTakeOut(int index = -1, qint32 overtime=10000);
    Q_INVOKABLE int tokenTakeOutCancel();
    //token take in
    Q_INVOKABLE int tokenTakeIn(QString target,qint32 overtime=10000);
    Q_INVOKABLE int tokenTakeIn(int index = -1,qint32 overtime=10000);
    Q_INVOKABLE int tokenTakeInCancel();
    //token order out
    Q_INVOKABLE int tokenOrderOutAck();
    Q_INVOKABLE int tokenOrderOutCancel();
    //token order in
    Q_INVOKABLE int tokenOrderInAck();
    Q_INVOKABLE int tokenOrderInCancel();
    //token take in forced
    Q_INVOKABLE int tokenForceTakeIn();
public slots:
    Q_INVOKABLE void tokenTakeOutOvertime();
    Q_INVOKABLE void tokenTakeInOvertime();
    Q_INVOKABLE void tokenOrderOutOvertime();
    Q_INVOKABLE void tokenOrderInOvertime();

private:
    int tokenTakeOutAck();
    int tokenTakeInAck();
    int tokenOrderOut(qint32 overtime=0);
    int tokenOrderIn(qint32 overtime=0);
    //token Order out forced
    int tokenForceOrderOut();

public:
    //token manager control
    Q_INVOKABLE int start(void);                    //新建端口，初始化网络连接，自身状态设置为online（启动失败除外）
    Q_INVOKABLE int stop(void);                     //删除端口，终止网络连接，自身状态设为Disable
    Q_INVOKABLE int restart(void);                  //重启

    Q_INVOKABLE bool isSelfFirstPriority();         //判断本peer是否最高优先级。
protected:
    int clearPeerInfo();//清空列表删除内存

signals:
    //主站同步指令更新
    void msgMasterPeerMessageUpdated();
    //扩展接口
    void msgOtherCommandReceived(QByteArray msg);
    //列表有更新
    void msgPeersListChanged(int index);

//get & set
public:
    QString getMasterPeerMessage() const;
    void setMasterPeerMessage(QString& msg);

    tmPeer* getPeer(QString& name) const;
    tmPeer* getPeer(int index) const;
    int getPeerIndex(QString& name) const;

    int setSelfPeer(tmPeer*newone);
    inline tmPeer* getSelfPeer() const;
    inline tmPeer* getPartner() const;

    int generateSelfInfo();

    bool isStarted();
    bool isIndexValid(int index);
private:
    int setPeer(tmPeer *newone); //网络上peer的信息写入


};

#endif // USRMANAGER_H
