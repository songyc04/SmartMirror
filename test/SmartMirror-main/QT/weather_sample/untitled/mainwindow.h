#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QNetworkAccessManager>
#include <QNetworkReply>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

    ~MainWindow();

private slots:

    void onWeatherReply(
        QNetworkReply *reply);

private:

    Ui::MainWindow *ui;

    QNetworkAccessManager *manager;

    void requestWeather(QString city);

    QString koreanCityName(QString city);
};

#endif
