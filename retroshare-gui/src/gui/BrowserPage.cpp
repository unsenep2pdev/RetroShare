#include "BrowserPage.h"
#include "ui_BrowserPage.h"

BrowserPage::BrowserPage(QWidget *parent) :
    MainPage(parent),
    ui(new Ui::BrowserPage)
{
    ui->setupUi(this);
}

BrowserPage::~BrowserPage()
{
    delete ui;
}
