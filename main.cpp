#include "showjcr.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ShowJCR w;
    //如果有命令行参数而且参数是开机自启动
    if (argc > 1 && (argv[1] == QString("autoStart"))){//注意参数没有空格
        w.hide();//不显示窗口
    } else {
        w.show();//正常显示
    }
    return a.exec();
}
