#ifndef WEATHERPANEL_H
#define WEATHERPANEL_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>

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
    void onForecastReply(QNetworkReply *reply);

    void onMidLandReply(QNetworkReply *reply);
    void onMidTempReply(QNetworkReply *reply);

private:
    Ui::WeatherPanel *ui;

    QNetworkAccessManager *manager;
    QNetworkAccessManager *forecastManager;

    // 중기예보용
    QNetworkAccessManager *midLandManager;
    QNetworkAccessManager *midTempManager;

    void requestWeather(QString city);
    void requestForecast();

    void requestMidLand();
    void requestMidTemp();

    QString koreanCityName(QString city);

    // 중기예보 데이터 저장
    QJsonObject midLandData;
    QJsonObject midTempData;

    void fillMidForecast();
};

#endif
