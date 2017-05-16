#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

#include "usrinfo.h"
/*
 * 构造函数
 * 输入参数：无
 * 返回数值：无
 * 功能描述：
 * 1、初始化密码为空
 * 2、设置密码
 * 3、初始化名称
 * 4、初始化等级
 */
UsrInfo::UsrInfo()
{
    _salt = "8y*P%";
    QByteArray initPswd = _salt.toLocal8Bit();
    _pswd = QCryptographicHash::hash(initPswd, QCryptographicHash::Sha256);
    _name = "undefined";
    _level = 0;

}

/*
 * 析构函数
 * 输入参数：无
 * 返回数值：无
 * 功能描述：
 */
UsrInfo::~UsrInfo(){

}

/*
 * 查询名称
 * 输入参数：无
 * 返回数值：用户名称
 * 功能描述：
 */
QString UsrInfo::getName() const{
    return _name;
}

/*
 * 查询等级
 * 输入参数：无
 * 返回数值：用户等级
 * 功能描述：
 */
quint8 UsrInfo::getLevel() const{
    return _level;
}

/*
 * 检查密码
 * 输入参数：
 * 1、testPswd 测试密码
 * 返回数值：
 * 1、正确返回ture，否则返回false
 * 功能描述：
 */
bool UsrInfo::passWordCheck(const QString& testPswd){
    QString temp = testPswd+_salt;
    QByteArray res = QCryptographicHash::hash(temp.toLatin1(), QCryptographicHash::Sha256);
    if(res == _pswd ){
        return true;
    }
    return false;
}

/*
 * 修改用户名
 * 输入参数：
 * 1、newName 新用户名
 * 2、testPswd 测试密码
 * 返回数值：
 * 1、成功返回ture，否则返回false
 * 功能描述：
 */
bool UsrInfo::setName(const QString& newName, const QString& testPswd){
    if(passWordCheck(testPswd)){
        _name = newName;
        return true;
    }
    return false;
}

/*
 * 修改等级
 * 输入参数：
 * 1、newLevel 新用户等级
 * 2、testPswd 测试密码
 * 返回数值：
 * 1、成功返回ture，否则返回false
 * 功能描述：
 */
bool UsrInfo::setLevel(const quint8& newLevel, const QString& testPswd){
    if(passWordCheck(testPswd)){
        _level = newLevel;
        return true;
    }
    return false;
}

/*
 * 修改密码
 * 输入参数：
 * 1、newPswd 新用户密码
 * 2、oldPswd 旧密码
 * 返回数值：
 * 1、成功返回ture，否则返回false
 * 功能描述：
 * 1、检查密码，正确就换新密码
 */
bool UsrInfo::setPassWord(const QString& oldPswd, const QString& newPswd){
    if(passWordCheck(oldPswd)){
        QString temp = newPswd + _salt;
        _pswd = QCryptographicHash::hash(temp.toLatin1(), QCryptographicHash::Sha256);
        return true;
    }
    return false;
}

/*
 * 保存参数
 * 输入参数：
 * 1、newPswd 新用户密码
 * 2、oldPswd 旧密码
 * 返回数值：
 * 1、成功返回ture，否则返回false
 * 功能描述：
 * 1、检查密码，正确就换新密码
 */
void UsrInfo::save(iLoadSaveProcessor* processor){

    processor->writeValue( QString("usrName"), _name );
    QString value = _pswd.toBase64();
    processor->writeValue( QString("usrPassword"), value );
    processor->writeValue( QString("usrLevel"), _level );
}

void UsrInfo::load(iLoadSaveProcessor* processor){
    QString value;
    bool ok;
    processor->readValue( QString("usrName"), _name );
    processor->readValue( QString("usrPassword"), value );
    _pswd = QByteArray::fromBase64( value.toLatin1() );
    processor->readValue( QString("usrLevel"), value );
    _level = value.toUShort(&ok, 10);
}
