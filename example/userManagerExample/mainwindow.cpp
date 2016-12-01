#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    _manager = new UsrManager(this);
    _model   = new QStandardItemModel(this);

    connect(_manager,SIGNAL(msgUsrListChanged()),this,SLOT(refreshData()));
    connect(_manager,SIGNAL(msgCurrentUsrChanged()),this,SLOT(refreshCurrent()));

    ui->tableView->setModel(_model);
}

MainWindow::~MainWindow()
{
    delete _manager;
    delete _model;
    delete ui;
}

void MainWindow::on_pbLogIn_clicked()
{
    QString msgText;
    bool ret = _manager->logIn( ui->leUsrName->text(), ui->leUsrPswd->text() );
    QMessageBox msgBox;
    if(ret) msgText = "log in successful!";
    else msgText = "log in failed!";
    msgBox.setText( msgText );
    msgBox.exec();
}

void MainWindow::on_pbLogOut_clicked()
{
    _manager->logOut();
}

void MainWindow::refreshData(){
    QList<UsrInfo*> list = _manager->getUsrList();
    QStandardItem* item;
    int i;


//    qDebug()<<"MainWindow::refreshData"<<_model->rowCount()<<list.size();
    for(i=0 ; i< list.size(); i++){
        item = _model->item(i,0);
        if(item){
            item->setText( list.at(i)->getName() );
        }
        else{
            item = new QStandardItem( list.at(i)->getName() );
        }
        _model->setItem(i,0,item);

        item = _model->item(i,1);
        if(item){
            item->setText( QString::number(list.at(i)->getLevel()) );
        }
        else{
            item = new QStandardItem( QString::number(list.at(i)->getLevel()) );
        }
        _model->setItem(i,1,item);
    }
    _model->removeRows(i, _model->rowCount() - i);
}

void MainWindow::refreshCurrent(){
    QString currentName;
    quint8 currentLevel;
    _manager->getNameAndLevel(&currentName,&currentLevel);
    ui->leCurrentUsrName->setText( currentName );
    ui->leCurrentUsrLevel->setText( QString::number( currentLevel ));

    ui->cbNewUsrLevel->clear();
    QStringList list;
    for(int i =1; i<= currentLevel; i++){
        list<<QString("level%1").arg(QString::number(i));
    }
    ui->cbNewUsrLevel->addItems(list);
}

void MainWindow::on_pbRefresh_clicked()
{
    refreshData();
}

void MainWindow::on_pbSetAutoLogOut_clicked()
{
    _manager->setAutoLogOut( ui->checkBox->isChecked(), ui->leAutoLogOutDelay->text().toInt() );
}

void MainWindow::on_pbDel_clicked()
{
    int currentRow = ui->tableView->currentIndex().row();
    QString name = _model->item(currentRow,0)->text();
    _manager->deleteUsr( name );
}


void MainWindow::on_pbAdd_clicked()
{
    QString newName = ui->leNewUsrName->text();
    QString newPswd = ui->leNewUsrPswd->text();
    quint8 newLevel = ui->cbNewUsrLevel->currentText().remove("level").toUInt();

    QString msgText;

    bool ret = _manager->addUsr(newName, newPswd, newLevel);

    QMessageBox msgBox;
    if(ret) msgText = "add usr successful!";
    else msgText = "add usr failed!";
    msgBox.setText( msgText );
    msgBox.exec();
}
