#ifndef USRINFO_H
#define USRINFO_H

#include <QObject>
#include <QCryptographicHash>
#include "iloadsave.h"
#include "iloadsaveprocessor.h"

class UsrInfo : public iLoadSave
{
public:
    explicit UsrInfo();
    ~UsrInfo();
    virtual void save(iLoadSaveProcessor* processor);
    virtual void load(iLoadSaveProcessor* processor);
    //查询名称
    QString getName(void) const;
    //查询等级
    quint8 getLevel(void) const;
    //检查密码，正确返回ture，否则返回false
    bool passWordCheck(const QString& testPswd);
    //修改用户名，成功返回ture，否则返回false
    bool setName(const QString& newName, const QString& testPswd);
    //修改等级，成功返回ture，否则返回false
    bool setLevel(const quint8& newLevel, const QString& testPswd);
    //修改密码，成功返回ture，否则返回false
    bool setPassWord(const QString& oldPswd, const QString& newPswd);

private:
    QString     _salt;
    QByteArray  _pswd;
    quint8      _level;
    QString     _name;
};

#endif // USRINFO_H
