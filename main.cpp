#include "showjcr.h"

#include <QApplication>
#include <QSharedMemory>
#include <QMessageBox>
#include <QMutex>
#include <QDateTime>
#include <QFile>
#include <QDir>

void outputMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg)
 {
     // 加锁
     static QMutex mutex;
     static QString logName = QApplication::applicationName() + "_log.txt";
     mutex.lock();
     QString text;
     switch(type)
     {
     case QtDebugMsg:
         text = QString("Debug:");
         break;
     case QtWarningMsg:
         text = QString("Warning:");
         break;
     case QtCriticalMsg:
         text = QString("Critical:");
         break;
     case QtFatalMsg:
         text = QString("Fatal:");
         break;
    default:break;
     }

     // 设置输出信息格式
     QString context_info = QString("File:(%1) Line:(%2)").arg(QString(context.file)).arg(context.line);
     QString current_date_time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ");
     QString current_date = QString("(%1)").arg(current_date_time);
     QString message;
#ifdef QT_DEBUG
     message = QString("%1 %2 %3 %4").arg(text).arg(context_info).arg(msg).arg(current_date);
#else
     message = QString("%1 %2").arg(current_date).arg(msg);
#endif
     // 输出信息至文件中（读写、追加形式）
     QFile file(QDir::temp().absoluteFilePath(logName));    //日志文件写在temp目录
     file.open(QIODevice::WriteOnly | QIODevice::Append);
     QTextStream text_stream(&file);
     text_stream << message << "\r\n";
     file.flush();
     file.close();
     // 解锁
     mutex.unlock();
 }

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //注册日志函数
    qInstallMessageHandler(outputMessage);
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
        qDebug() << argv[1];
        w.hide();//不显示窗口
    } else {
        w.show();//正常显示
    }
    return a.exec();
}
