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

struct RegionInfo
{
    QString name;
    int nx;
    int ny;
    QString shortTermRegId;
    QString midLandRegId;
    QString midTempRegId;
};

class WeatherPanel : public QWidget
{
    Q_OBJECT

public:
    explicit WeatherPanel(QWidget *parent = nullptr);
    ~WeatherPanel();

    void setTextBrightness(int value);
    void setRegion(int regionCode);

private slots:
    void onWeatherReply(QNetworkReply *reply);
    void onMidLandReply(QNetworkReply *reply);
    void onMidTempReply(QNetworkReply *reply);
    void onShortTermReply(QNetworkReply *reply);
    void onDailyForecastReply(QNetworkReply *reply);

private:
    Ui::WeatherPanel *ui;

    QNetworkAccessManager *manager;
    QNetworkAccessManager *midLandManager;
    QNetworkAccessManager *midTempManager;
    QNetworkAccessManager *shortTermManager;
    QNetworkAccessManager *dailyManager;

    void requestWeather();
    void requestMidLand();
    void requestMidTemp();
    void requestShortTerm();
    void requestDailyForecast();

    void tryFillMidForecast();

    QJsonObject midLandData;
    QJsonObject midTempData;

    RegionInfo currentRegion;

    double currentTemp = 0.0;
    int todayMinTemp = -999;
    int todayMaxTemp = -999;
};



#endif
