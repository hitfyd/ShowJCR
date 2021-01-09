#include "showjcr.h"

#include <QApplication>
#include <QSharedMemory>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ShowJCR w;
    //设置程序单启动，使用共享内存创建的同时设置key,也可以setKey
    QSharedMemory sm(a.applicationName());
    if(sm.attach()){
        qDebug() << "SingleApp is running!";
        QMessageBox::warning(&w, "程序正在运行", "程序已经启动，请不要重复运行！请检查任务栏或系统托盘！");
        return 0;
    }
    sm.create(1);
    //如果有命令行参数而且参数是开机自启动
    if (argc > 1 && (argv[1] == QString("autoStart"))){//注意参数没有空格
        w.hide();//不显示窗口
    } else {
        w.show();//正常显示
    }
    return a.exec();
}
