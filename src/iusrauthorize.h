#ifndef IUSRAUTHRIZE
#define IUSRAUTHRIZE
#include <QObject>

class IUsrAuthorize
{
public:
    explicit IUsrAuthorize(){}
    virtual ~IUsrAuthorize(){}
    //登录
    virtual bool logIn(const QString& name, const QString& password)=0;
    //登出
    virtual bool logOut( void )=0;
    //设置自动登出功能
    virtual bool setAutoLogOut(bool en, int durationSec)=0;
    //查询自动登出功能
    virtual void getAutoLogOut(bool* en, int* durationSec) const =0;
    //查询用户名和级别
    virtual bool getNameAndLevel(QString *name, quint8 *level ) const=0;

};


#endif // IUSRAUTHRIZE

