#ifndef NEWSPANEL_H
#define NEWSPANEL_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

namespace Ui {
class NewsPanel;
}

class NewsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit NewsPanel(QWidget *parent = nullptr);
    ~NewsPanel();
    void setTextBrightness(int value);

private slots:
    void onNewsReply(QNetworkReply *reply);

private:
    Ui::NewsPanel *ui;

    QNetworkAccessManager *manager;
    QTimer *refreshTimer;

    void requestNews();
    void clearNewsLayout();
};

#endif // NEWSPANEL_H
