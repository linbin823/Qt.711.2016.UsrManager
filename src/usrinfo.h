#ifndef USRINFO_H
#define USRINFO_H

#include <QObject>
#include <QTime>
#include <QTimer>
#include <QHostAddress>
#include <QNetworkInterface>
#include "basedevice.h"
#include "iloadsaveprocessor.h"

class UsrInfo : public baseDevice
{
    Q_OBJECT
public:
    explicit UsrInfo(QObject *parent = 0);

    enum state: quint64{
        stateDisable                   = 0x0000000000000001ULL,      //peer禁用
        stateOnlinewithToken           = 0x0000000000000002ULL,      //peer在线有令牌，主站
        stateOnlinewithoutToken        = 0x0000000000000004ULL,      //peer在线无令牌，从站
        stateTokenTakeOutPending       = 0x0000000000000008ULL,      //该peer主动把令牌给其他peer，但其他peer还未确认。该peer具有令牌，主站。
        stateTokenOrderOutPending      = 0x0000000000000010ULL,      //其他peer要求该peer的令牌，该peer还未确认。该peer具有令牌，主站。
        stateTokenTakeInPending        = 0x0000000000000020ULL,      //该peer主动要获得令牌，但令牌持有peer还未确认。该peer无令牌，从站。
        stateTokenOrderInPending       = 0x0000000000000040ULL,      //令牌持有peer要求把令牌传送给该peer，但该peer还未确认。该peer无令牌，从站。
        stateOffline                   = 0x0000000000000080ULL,      //peer离线。此状态不适用于本站，用于判断其他peer的掉线状态。
    };
    virtual  QString getStateString(int lang = 0 ) const{
        if(lang == langEN){
             switch(getState()){
             case stateDisable:
                 return QString("stop");
             case stateOnlinewithToken:
                 return QString("online with token");
             case stateOnlinewithoutToken:
                 return QString("online without token");
             case stateTokenTakeOutPending:
                 return QString("waiting for token gave out");
             case stateTokenTakeInPending:
                 return QString("waiting for token toke in");
             case stateTokenOrderOutPending:
                 return QString("waiting for token toke out");
             case stateTokenOrderInPending:
                 return QString("waiting for token gave in");
             case stateOffline:
                 return QString("offline");
             default:
                 return QString("error code not found");
             }
        }
        else if(lang == langCHN){
            switch(getState()){
            case stateDisable:
                return tr("停用");
            case stateOnlinewithToken:
                return tr("在线有令牌");
            case stateOnlinewithoutToken:
                return tr("在线无令牌");
            case stateTokenTakeOutPending:
                return tr("主动转出 等待确认");
            case stateTokenTakeInPending:
                return tr("主动转入 等待确认");
            case stateTokenOrderOutPending:
                return tr("被动转出 等待确认");
            case stateTokenOrderInPending:
                return tr("被动转入 等待确认");
            case stateOffline:
                return tr("掉线");
            default:
                return tr("找不到错误代码");
            }
        }
     }
    enum errorType : quint64 {
        errorInnerError                = 0x0000000000000001ULL,      //peer内部故障，致命必须重启
        errorTakeOutOverTime           = 0x0000000000000002ULL,
        errorTakeInOverTime            = 0x0000000000000004ULL,
        errorOrderOutOverTime          = 0x0000000000000008ULL,
        errorOrderInOverTime           = 0x0000000000000010ULL,
    };
    virtual QString getErrorString(quint64 errorCode, int lang = 0 ) const{
        if(lang == langEN){
            switch(errorCode){
                case errorInnerError:
                    //qDebug()<<"tmPeer::getErrorString 2#";
                    return QString("inner error");
                case errorTakeOutOverTime:
                    return QString("token give out timeover");//gave out
                case errorTakeInOverTime:
                    return QString("token take in timeover");//take in
                case errorOrderOutOverTime:
                    return QString("token take out timeover");//take out
                case errorOrderInOverTime:
                    return QString("token give in timeover");//give in
                default:
                    return QString("error code not found");
            }
        }
        else if(lang == langCHN){
            switch(errorCode){
                case errorInnerError:
                    //qDebug()<<"tmPeer::getErrorString 2#";
                    return tr("内部错误");
                case errorTakeOutOverTime:
                    return tr("主动转出等待超时");
                case errorTakeInOverTime:
                    return tr("主动转入等待超时");
                case errorOrderOutOverTime:
                    return tr("被动转出等待超时");
                case errorOrderInOverTime:
                    return tr("被动转入等待超时");
                default:
                    return tr("找不到错误代码");
            }
        }
    }

    virtual int save(iLoadSaveProcessor* processor);
    virtual int load(iLoadSaveProcessor* processor);


public:
    void setPeerPriority(quint32 pri);
    void setPeerIp(quint32 ip);

    quint32 getPeerPriority() const;
    quint32 getPeerIp() const;

    void update(const QString& name, quint64 state, quint64 error, quint32 pri, quint32 ip);
    void update();
    bool isWithToken();  //判断是否有令牌，有=true

    bool stopCheckOffline();//对于本peer，不用检查掉线

signals:
    void msgPriorityChanged(qint32 pri);
    void msgIpChanged(qint32 ip);

private slots:
    void offlineCheck();
//    void peerChanged();

private:
    quint32 peerIp;
    quint32 priority;
    QTime lastUpdateTime;

    QTimer offlineCheckTimer;
    int offlineDelay;
    int offlineCheckInterv;

};

#endif // USRINFO_H
