#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

#include "usrmanager.h"
#include "loadsaveprocessorjson.h"
UsrManager* UsrManager::_singleton = nullptr;

UsrManager* UsrManager::getUsrManager(){
    if(_singleton == nullptr){
        _singleton = new UsrManager();
    }
    return _singleton;
}

/*
 * 构造函数
 * 输入参数：父QObject
 * 返回数值：无
 * 功能描述：
 * 1、新建保存器
 * 2、设置密码
 * 3、初始化自动登出定时器
 * 4、读取数据
 */
UsrManager::UsrManager(QObject *parent) : QObject(parent)
{
    _autoLogOutTimer.setSingleShot(true);
    connect(&_autoLogOutTimer,SIGNAL(timeout()),this,SLOT(autoLogOut()));

    _autoLogOutEn =false;
    _autoLogOutSec = 0;

    load();

    emit msgCurrentUsrChanged();
    emit msgUsrListChanged();
}
/*
 * 析构函数
 * 输入参数：无
 * 返回数值：无
 * 功能描述：
 * 1、保存所有数值
 * 2、删除保存器
 * 3、停止定时器
 */
UsrManager::~UsrManager(){
    save();
    _autoLogOutTimer.stop();
}


/*
 * 登入
 * 输入参数：用户名、明文密码
 * 返回数值：
 * 1、是否成功，1成功 0失败
 * 功能描述：
 * 1、把用户名密码记录到内部
 * 2、启动自动登出计时器
 * 3、检查是否登录成功
 */
bool UsrManager::logIn(const QString& name, const QString& password){
    _currentName = name;
    _currentPassword = password;
    emit msgCurrentUsrChanged();//当前用户变化
    emit msgUsrListChanged();//当前用户列表变化
    if(_autoLogOutEn){
        _autoLogOutTimer.start( _autoLogOutSec * 60 * 1000 );
    }
    return checkLogIn();
}

//自动登出
void UsrManager::autoLogOut(){
    logOut();
}

/*
 * 登出
 * 输入参数：无
 * 返回数值：
 * 1、是否成功，1成功
 * 功能描述：
 * 1、删除当前用户名密码
 * 2、停止计时器
 */
bool UsrManager::logOut( void ){
    _currentName = QString::null;
    _currentPassword = QString::null;
    _autoLogOutTimer.stop();
    emit msgCurrentUsrChanged();
    emit msgUsrListChanged();
    return true;
}

/*
 * 设置自动登出功能
 * 输入参数：
 * 1、en 是否激活该功能
 * 2、durationSec 自动登出的时间（s）
 * 返回数值：
 * 1、是否修改，1成功 0未修改（时间值无效）
 * 功能描述：
 * 1、如果激活且登出时间有效，则修改
 * 2、如果不激活，则只修改en位
 */
bool UsrManager::setAutoLogOut(bool en, int durationSec){
    if(en && durationSec>0){
        _autoLogOutEn = en;
        _autoLogOutSec = durationSec;
        if( checkLogIn() ){
            //登录成功，则要重启定时器
            _autoLogOutTimer.start( _autoLogOutSec * 60 * 1000 );
        }
        return true;
    }
    else if(!en){
        _autoLogOutEn = en;
        _autoLogOutTimer.stop();
        return true;
    }
    else{
        return false;
    }
}

/*
 * 查询自动登出功能
 * 输入参数：（输出参数）
 * 1、en 是否激活该功能
 * 2、durationSec 自动登出的时间（s）
 * 返回数值：无
 * 功能描述：
 */
void UsrManager::getAutoLogOut(bool* en, int* durationSec) const{
    if(en){
        *en = _autoLogOutEn;
    }
    if(durationSec){
        *durationSec = _autoLogOutSec;
    }
}

/*
 * 查询登陆的用户名和级别
 * 输入参数：无
 * 返回数值：
 * 1、是否是有效登录
 * 功能描述：
 * 1、遍历所有的用户，查找登录的名称和级别
 * 2、如果没有有效登录，则返回"anonymous"和levelAnonymous
 * 3、anonymous用户不存在于_usrList列表中。当登录失败或没有登录，默认就是anonymous用户登录
 */
bool UsrManager::getNameAndLevel(QString* name, quint8* level ) const{
    QListIterator<UsrInfo*> it(_usrList);
    UsrInfo* temp;
    it.toFront();
    while(it.hasNext()){
        temp = it.next();
        if(temp->getName() == _currentName ){
            if( temp->passWordCheck( _currentPassword ) ){
                //用户名正确密码匹配，
                if(name) *name = _currentName;
                if(level) *level = temp->getLevel();
                return true;
            }
        }
    }
    if(name) *name = "Anonymous";
    if(level) *level = levelAnonymous;
    return false;
}

/*
 * 添加用户
 * 输入参数：
 * 1、用户名称
 * 2、用户密码
 * 3、用户等级
 * 返回数值：
 * 1、是否添加成功
 * 功能描述：
 * 1、等级是否满足要求
 * 2、添加
 * 3、anonymous用户不存在于_usrList列表中。anonymous用户的level = 0 也不能被其他用户占用。
 */
bool UsrManager::addUsr(const QString& name, const QString& pswd, const quint8& level){
    UsrInfo* pTemp = new UsrInfo();
    quint8 currentLevel;
    getNameAndLevel( 0, &currentLevel);
    //当前登录等级比新建用户等级低，或者新的level小于等于0，则不能新建，
    if(currentLevel < level || level <=0 ) return false;

    //有重名(返回false)，不能新建
    if( !checkName( name ) )  return false;

    bool ret = pTemp->setName(name, "");
    ret = pTemp->setLevel(level, "");
    ret = pTemp->setPassWord( "", pswd);
    _usrList.append(pTemp);
    emit msgUsrListChanged();
    return true;
}


/*
 * 删除用户
 * 输入参数：
 * 1、用户名称
 * 2、用户密码
 * 返回数值：
 * 1、是否删除成功
 * 功能描述：
 * 1、等级是否满足要求
 * 2、删除
 * 3、anonymous用户不存在于_usrList列表中。所以anonymous用户也不能删除
 */
bool UsrManager::deleteUsr(const QString& name){
    QListIterator<UsrInfo*> it(_usrList);
    UsrInfo* temp;
    it.toFront();
    while(it.hasNext()){
        temp = it.next();
        if(temp->getName() == name ){
            //用户名匹配
            quint8 currentLevel;
            getNameAndLevel( 0, &currentLevel);
            //当前登录等级比要删除的用户等级低，则不能删除
            if(currentLevel < temp->getLevel() ) return false;

            _usrList.removeOne( temp );
            delete temp;
            emit msgUsrListChanged();
            emit msgCurrentUsrChanged();
            return true;
        }
    }
    return false;
}

/*
 * 获得用户列表
 * 输入参数：无
 * 返回数值：
 * 1、等于或低于当前登录等级的所有用户的集合
 * 功能描述：
 * 1、返回用户列表
 * 2、anonymous用户不存在于_usrList列表中。所以不会看到anonymous用户
 */
QList<UsrInfo *> UsrManager::getUsrList(){
    QList<UsrInfo*> ret;
    QListIterator<UsrInfo*> it(_usrList);
    UsrInfo* temp;
    quint8 currentLevel;
    getNameAndLevel( 0, &currentLevel);

    it.toFront();
    while(it.hasNext()){
        temp = it.next();
        if( temp->getLevel() <= currentLevel ){
            ret<<temp;
        }
    }
    return ret;
}

/*
 * 获得用户
 * 输入参数：
 * 1、用户名
 * 返回数值：
 * 1、用户指针
 * 功能描述：
 * 1、找不到返回0
 */
UsrInfo* getUsr(const QString& name){
    QListIterator<UsrInfo*> it(_usrList);
    UsrInfo* temp;
    it.toFront();
    while(it.hasNext()){
        temp = it.next();
        if(temp->getName() == name ){
            //用户名匹配
            return temp;
        }
    }
    return nullptr;
}

/*
 * 获得用户列表
 * 输入参数：无
 * 返回数值：
 * 1、等于或低于当前登录等级的所有用户的集合
 * 功能描述：
 * 1、返回用户列表
 * 2、anonymous用户不存在于_usrList列表中。所以不会看到anonymous用户
 */
QList<UsrInfo *> UsrManager::getUsrList(){
    QList<UsrInfo*> ret;
    QListIterator<UsrInfo*> it(_usrList);
    UsrInfo* temp;
    quint8 currentLevel;
    getNameAndLevel( 0, &currentLevel);

    it.toFront();
    while(it.hasNext()){
        temp = it.next();
        if( temp->getLevel() <= currentLevel ){
            ret<<temp;
        }
    }
    return ret;
}

/*
 * 检查是否登录成功
 * 输入参数：无
 * 返回数值：
 * 1、是否登录成功
 * 功能描述：
 * 1、当登录失败或没有登录，默认就是anonymous用户登录，也是return false
 */
bool UsrManager::checkLogIn(void) const{
    QListIterator<UsrInfo*> it(_usrList);
    UsrInfo* temp;
    it.toFront();
    while(it.hasNext()){
        temp = it.next();
        if(temp->getName() == _currentName ){
            if( temp->passWordCheck( _currentPassword ) ){
                //用户名正确密码匹配，
                return true;
            }
        }
    }
    return false;
}

/*
 * 检查用户名是否重名
 * 输入参数：
 * 1、新的用户名称
 * 返回数值：
 * 1、重名=false，无重名=true
 * 功能描述：
 * 1、遍历所有用户，检查新的用户名是否重名
 */
bool UsrManager::checkName(const QString& newName) const{
    QListIterator<UsrInfo*> it(_usrList);
    UsrInfo* temp;
    it.toFront();
    while(it.hasNext()){
        temp = it.next();
        if(temp->getName() == newName ){
                return false;
        }
    }
    return true;
}


int UsrManager::load(){
    QString paraValue;
    UsrInfo* usr;
    int ret;

    iLoadSaveProcessor* _processor;
    _processor = new loadSaveProcessorJson(this, true);
    _processor->setPassWord("usr_for_joystick");


    ret = _processor->transactionStart();
    if(ret !=0 ){
        insetLevelSuperior();
        _processor->transactionEnd();
        delete _processor;
        return -1;
    }
    ret = _processor->loadFile("usr._x");
    if(ret !=0 ){
        insetLevelSuperior();
        _processor->transactionEnd();
        delete _processor;
        return -1;
    }
    _processor->loadParameters("autoLogOutEn",&paraValue);
    _autoLogOutEn = paraValue.toInt();
    _processor->loadParameters("autoLogOutSec",&paraValue);
    _autoLogOutSec = paraValue.toInt();

    _processor->loadParameters("usrCounts",&paraValue);
    int count = paraValue.toInt();
    for(int i=0; i<count; i++){
        //load的时候要检查用户名是否重复，重复的话要丢弃

        _processor->moveToInstance("usrInfo", QString::number(i));
        usr = new UsrInfo();
        usr->load(_processor);
        _processor->moveBackToParent();

        if( !checkName( usr->getName() ) ){
            delete usr;
            continue;
        }
        _usrList.append(usr);
    }
    _processor->transactionEnd();
    delete _processor;
    return 0;
}

int UsrManager::save(){

    int ret;

    iLoadSaveProcessor* _processor;
    _processor = new loadSaveProcessorJson(this, true);
    _processor->setPassWord("usr_for_joystick");


    ret =  _processor->transactionStart();
    if(ret !=0 ){
        _processor->transactionEnd();
        delete _processor;
        return -1;
    }
    _processor->saveParameters("autoLogOutEn", QString::number(_autoLogOutEn));
    _processor->saveParameters("autoLogOutSec",QString::number(_autoLogOutSec));
    _processor->saveParameters("usrCounts",QString::number(_usrList.size()));

    for(int i=0; i<_usrList.size(); i++){
        _processor->moveToInstance("usrInfo", QString::number(i));
        _usrList.at(i)->save(_processor);
        _processor->moveBackToParent();
    }

    ret =  _processor->saveFile("usr._x");
    if(ret !=0 ){
        _processor->transactionEnd();
        delete _processor;
        return -1;
    }
    _processor->transactionEnd();
    delete _processor;
    return 0;
}

void UsrManager::insetLevelSuperior(){
    UsrInfo* superior;
    superior = new UsrInfo();
    superior->setLevel(levelSuperior, "");
    superior->setName("superior","");
    superior->setPassWord("","supp1");
    _usrList.append(superior);
}
