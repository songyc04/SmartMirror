#ifndef NEWSPANEL_H
#define NEWSPANEL_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace Ui {
class NewsPanel;
}

class NewsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit NewsPanel(QWidget *parent = nullptr);
    ~NewsPanel();

private slots:
    void onNewsReply(QNetworkReply *reply);

private:
    Ui::NewsPanel *ui;

    QNetworkAccessManager *manager;

    void requestNews();
};

#endif // NEWSPANEL_H
