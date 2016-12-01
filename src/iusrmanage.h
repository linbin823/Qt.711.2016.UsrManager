#ifndef IUSRMANAGE
#define IUSRMANAGE
#include <QObject>
#include <QList>
#include "usrinfo.h"

class IUsrManage
{
public:
    explicit IUsrManage(){}
    virtual ~IUsrManage(){}
    //添加用户
    virtual bool addUsr(const QString& name, const QString& pswd, const quint8& level) = 0;
    //删除用户
    virtual bool deleteUsr(const QString& name) = 0;
    //获得用户列表
    virtual QList<UsrInfo*> getUsrList() = 0;
};
#endif // IUSRMANAGE

