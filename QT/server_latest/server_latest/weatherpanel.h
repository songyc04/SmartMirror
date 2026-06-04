#ifndef WEATHERPANEL_H
#define WEATHERPANEL_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>

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
    void onWeatherReply(
        QNetworkReply *reply);

private:
    Ui::WeatherPanel *ui;
    QNetworkAccessManager *manager;
    void requestWeather(QString city);
    QString koreanCityName(QString city);
};

#endif // WEATHERPANEL_H
