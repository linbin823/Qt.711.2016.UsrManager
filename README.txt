用户管理器 2016.11.11
规划功能
1、用户添加、改权限、用户名密码修改
2、提供log in功能
3、自动log out功能
4、密码使用harsh散列存储
5、所有用户资料加密存储
6、用户资料内容：
    用户名、权级、密码harsh值
    用户信息目前只含权级（int64），由低到高，表示用户的权限

7、管理器含有当前用户名、含有密码两个变量（QString），含有一个公共方法int getLevel()，实时验证用户名和密码是否正确。以及log的等级。
8、usrManager的接口：
    IUsrManag，添加、删除、修改用户条目
    IUsrAuthrize，运行时查询当前level，log in，log out，auto log out
    ILoadSave，加密保存所有用户资料

TODO
1、权限等级的管理，新增、删除等
