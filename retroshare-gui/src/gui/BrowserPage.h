#ifndef BROWSERPAGE_H
#define BROWSERPAGE_H

#include <retroshare-gui/mainpage.h>
#include <retroshare/rsfiles.h>
#include <retroshare/rspeers.h>


#include <QWidget>

namespace Ui {
class BrowserPage;
}

class BrowserPage : public MainPage
{
    Q_OBJECT

public:
    explicit BrowserPage(QWidget *parent = nullptr);
    ~BrowserPage();

    virtual QIcon iconPixmap() const { return QPixmap(":/home/img/Setting/Browser.png") ; }
    virtual QString pageName() const { return tr("Brower") ; }
    virtual QString helpText() const { return ""; } //MainPage


private:
    Ui::BrowserPage *ui;
};

#endif // BROWSERPAGE_H
