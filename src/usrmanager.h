#ifndef USRMANAGER_H
#define USRMANAGER_H

#include <QObject>
#include <QList>
#include <QTimer>

#include "iloadsaveprocessor.h"

#include "iusrauthorize.h"
#include "iusrmanage.h"
#include "usrinfo.h"

class UsrManager  : public QObject, public IUsrAuthorize, public IUsrManage
{
    Q_OBJECT
private:
    explicit UsrManager(QObject *parent = 0);
    //单例设计
    static UsrManager* _singleton;

public:
    static UsrManager* getUsrManager();

    ~UsrManager();
    //等级设计
    enum level: quint8{
        levelSuperior                    = 0xFF,      //工程权限
        levelMantainance                 = 0x03,      //最高权限，维护工程师
        levelAdministartor               = 0x02,      //管理员权限
        levelOperator                    = 0x01,      //操作员权限
        levelAnonymous                   = 0x00,      //未登录
    };


    //登录
    virtual bool logIn(const QString& name, const QString& password);
    //登出
    virtual bool logOut( void );
    //设置自动登出功能
    virtual bool setAutoLogOut(bool en, int durationSec = 0);
    //查询自动登出功能
    virtual void getAutoLogOut(bool* en, int* durationSec) const;
    //查询用户名和级别
    virtual bool getNameAndLevel(QString *name, quint8 *level ) const;

    //添加用户
    virtual bool addUsr(const QString& name, const QString& pswd, const quint8 &level);
    //删除用户
    virtual bool deleteUsr(const QString& name);
    //获得用户列表
    virtual QList<UsrInfo*> getUsrList();
    //获取用户
    virtual UsrInfo* getUsr(const QString& name);

signals:
    void msgCurrentUsrChanged();
    void msgUsrListChanged();

private:

    QList<UsrInfo*> _usrList;
    QString _currentName;
    QString _currentPassword;

    QString _configFilePath;

    int _autoLogOutSec;
    bool _autoLogOutEn;
    QTimer _autoLogOutTimer;

    //检查登录是否成功
    bool checkLogIn(void) const;
    //检查用户名是否重名
    bool checkName(const QString &newName) const;
    int save();
    int load();

    void insetLevelSuperior();

private slots:
    //自动登出
    void autoLogOut();
};

#endif // USRMANAGER_H
