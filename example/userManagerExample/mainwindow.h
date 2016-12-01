#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QStandardItemModel>
#include "usrmanager.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_pbLogIn_clicked();

    void on_pbLogOut_clicked();
    void refreshData();
    void refreshCurrent();

    void on_pbRefresh_clicked();

    void on_pbSetAutoLogOut_clicked();

    void on_pbDel_clicked();

    void on_pbAdd_clicked();

private:
    Ui::MainWindow *ui;
    UsrManager* _manager;

    QStandardItemModel* _model;

};

#endif // MAINWINDOW_H
