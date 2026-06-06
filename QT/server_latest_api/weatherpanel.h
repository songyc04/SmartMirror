#ifndef WEATHERPANEL_H
#define WEATHERPANEL_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QTextCodec>

namespace Ui {
class WeatherPanel;
}

class WeatherPanel : public QWidget
{
    Q_OBJECT

public:
    explicit WeatherPanel(QWidget *parent = nullptr);
    ~WeatherPanel();

    void setTextBrightness(int value);

private slots:
    void onWeatherReply(QNetworkReply *reply);
    void onMidLandReply(QNetworkReply *reply);
    void onMidTempReply(QNetworkReply *reply);
    void onShortTermReply(QNetworkReply *reply);   // ← 추가

private:
    Ui::WeatherPanel *ui;

    QNetworkAccessManager *manager;
    QNetworkAccessManager *midLandManager;
    QNetworkAccessManager *midTempManager;
    QNetworkAccessManager *shortTermManager;       // ← 추가

    void requestWeather();
    void requestMidLand();
    void requestMidTemp();
    void requestShortTerm();                       // ← 추가

    void tryFillMidForecast();

    QJsonObject midLandData;
    QJsonObject midTempData;

    double currentTemp = 0.0;
};

#endif
