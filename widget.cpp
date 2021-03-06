#include "widget.h"
#include "ui_widget.h"

#include <QUrl>
#include <QDebug>
#include <QRegExp>
#include <QMessageBox>
#include <QFileDialog>

#include <QtGlobal>
#if QT_VERSION >= 0x050000
    #include <QWebFrame>
#else
    #include <QtWebKit/QWebFrame>
#endif

#include <algorithm>

#define MULTI_LINE_STRING(a) #a

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    pageCount = filesCount = 0;
    fileCNT = 0;
    pgtCNT = 1;
    stackSize = 0;
    directLink = lastPage = false;
    filesOnPage = 18;

    textLog = new QFile(this);
    textLog->setFileName("SW-Log.txt");

    webPage = new WebPage(this);
    ui->webView->setPage(webPage);

    timer = new QTimer(this);

    mainFrame = ui->webView->page()->mainFrame();

    ui->tableWidget->setRowCount(100);
    ui->tableWidget->setColumnCount(4);
    QStringList labels;
    labels << "Name" << "Link" << "Direct Links" << "MD5 chksm";
    ui->tableWidget->setHorizontalHeaderLabels(labels);

    setWindowState(Qt::WindowMaximized);

    ui->webView->setDisabled(true);

    connect(mainFrame, SIGNAL(loadFinished(bool)), this, SLOT(webPageLoaded(bool)));
    connect(ui->pushButton, SIGNAL(clicked(bool)), this, SLOT(goToURL()));
    connect(ui->webView, SIGNAL(loadProgress(int)), ui->progressBar, SLOT(setValue(int)));
    connect(timer, SIGNAL(timeout()), this, SLOT(timerOff()));
}

Widget::~Widget()
{
    delete ui;
}

void Widget::webPageLoaded(bool)
{
    qDebug() << "Page Loaded!";

    if (!directLink) {
        pageCount = getPageCount();
        filesCount = getFilesCount();
        qDebug() << "Files: " << filesCount << "Pages: " << pageCount;
        ui->tableWidget->setRowCount(filesCount);
    }

    ui->label_2->setText(tr("Getting links..."));

    if (pageCount && filesCount && !directLink) {
        const char* theJavaScriptCodeToInject = MULTI_LINE_STRING(
                function gettingLinks()
                {
                    var stackLinks = [];
                    var allSpans = document.getElementsByClassName("list-group-item-heading");
                    for (theSpan in allSpans)
                    {
                        var link = allSpans[theSpan].innerHTML;
                        if (link)
                        {
                            link = link.replace("<i class=\"fa fa-file-archive-o\"></i> ", "");
                            stackLinks.push(link);
                        }
                    }
                    return stackLinks;
                }
                gettingLinks();
            );
        QVariant jsReturn = mainFrame->evaluateJavaScript(theJavaScriptCodeToInject);

        fillTableLinks(jsReturn.toStringList());
    }

    if (directLink) {
        const char* clickJsCode = MULTI_LINE_STRING(
                function clickMirrorButton()
                {
                    document.getElementById('loadMirror').click();
                }
                clickMirrorButton();
            );
        mainFrame->evaluateJavaScript(clickJsCode);
        timer->start(ui->spinBox->value() * 1000);
    }
}

void Widget::goToURL()
{
    ui->label_2->setText("Loading page...");
    ui->webView->load(QUrl(ui->lineEdit->text()));
}

void Widget::fillTableLinks(const QStringList &stackLinks)
{
    stackSize = stackLinks.length();

    for (int i = 0; i < stackLinks.length(); ++i) {
        QString string = stackLinks[i];

        QString name = string;
        name.remove(QRegExp("<[^>]*>"));

        QString link = string;
        link.remove("<a href=\"");
        link = link.left(23);

        ui->tableWidget->setItem(i + getArg(), 0, new QTableWidgetItem(name));
        ui->tableWidget->setItem(i + getArg(), 1, new QTableWidgetItem("https://androidfilehost.com" + link));
    }

    directLink = true;

    ui->lineEdit->setText(ui->tableWidget->item(fileCNT + getArg(), 1)->text());
    goToURL();
}

int Widget::getPageCount() const
{
    ui->label_2->setText("Pages count is...");

    const char* theJavaScriptCodeToInject = MULTI_LINE_STRING(
                function getPagesCount()
                {
                    var stackLinks = [];
                    var allSpans = document.getElementsByClassName("pagination navbar-right");
                    for (theSpan in allSpans)
                    {
                        var link = allSpans[theSpan].innerHTML;
                        if (link)
                        {
                            stackLinks.push(link);
                        }
                    }
                    return stackLinks;
                }
                getPagesCount();
            );

    QVariant jsReturn = mainFrame->evaluateJavaScript(theJavaScriptCodeToInject);

    QString s = jsReturn.toStringList()[0].replace(QRegExp("<[^>]*>"), "a");
    QString pageCnt = "";
    bool flag = false;
    for (int i = s.length(); i >= 0; --i) {
        if (s[i].isDigit()) {
            pageCnt += s[i];
            flag = true;
        } else {
            if (flag) {
                break;
            }
        }
    }

    std::reverse(pageCnt.begin(), pageCnt.end());

    int pgCnt = pageCnt.toInt();

    ui->label_2->setText(QString("Pages count is... %1.").arg(pgCnt));

    return pgCnt;
}

int Widget::getFilesCount() const
{
    const char* theJavaScriptCodeToInject = MULTI_LINE_STRING(
                function getFilesNum()
                {
                    var stackLinks = [];
                    var allSpans = document.getElementsByClassName("navbar-brand");
                    for (theSpan in allSpans)
                    {
                        var link = allSpans[theSpan].innerHTML;
                        if (link)
                        {
                            stackLinks.push(link);
                        }
                    }
                    return stackLinks;
                }
                getFilesNum();
            );

    QVariant jsReturn = mainFrame->evaluateJavaScript(theJavaScriptCodeToInject);

    QString s = jsReturn.toStringList()[1].replace("files", "").trimmed();

    return s.toInt();
}

void Widget::flushToFile(int begin, int end)
{
    if (!(textLog->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))) {
        qDebug() << "Error open file!";
    }

    QTextStream out(textLog);
    for(int i = begin; i < end; ++i)
    {
        out << "Name: " << ui->tableWidget->item(i, 0)->text() << "\n";
        out << "Link: " << ui->tableWidget->item(i, 1)->text() << "\n";
        out << "MD5: " << ui->tableWidget->item(i, 3)->text() << "\n";
        out << "Direct Links:\n" << ui->tableWidget->item(i, 2)->text() << "\n\n\n";
    }
    textLog->close();
}

void Widget::tableDirectLinks(const QString &links)
{
    QString dlinks = links;
    QString buffer = "";
    dlinks = dlinks.trimmed();

    QStringList linksList = dlinks.split("<a href=\"");
    linksList.removeAt(0);
    for (int i = 0; i < linksList.length(); ++i) {
        int r = linksList[i].indexOf(".zip");
        buffer += linksList[i].left(r) + ".zip\n";
    }

    ui->tableWidget->setItem(fileCNT + getArg(), 2, new QTableWidgetItem(buffer));
}

void Widget::md5ToTable(const QString &md5)
{
    ui->tableWidget->setItem(fileCNT + getArg(), 3, new QTableWidgetItem(md5));
}

int Widget::getArg() const
{
    if (!lastPage) {
        return (pgtCNT - 1) * stackSize;
    } else {
        int lastPagesFilesCount = filesCount % filesOnPage;
        int allFilesOnFullPage = filesCount - lastPagesFilesCount;
        return allFilesOnFullPage;
    }
}

void Widget::timerOff()
{
    // Get Direct Links
    const char* clickJsCode_1 = MULTI_LINE_STRING(
                function getDirectLinks()
                {
                    var stackLinks = [];
                    var allSpans = document.getElementsByClassName("list-group");
                    for (theSpan in allSpans)
                    {
                        var link = allSpans[theSpan].innerHTML;
                        if (link)
                        {
                            stackLinks.push(link);
                        }
                    }
                    return stackLinks;
                }
                getDirectLinks();
        );
    QVariant jsRet_1 = mainFrame->evaluateJavaScript(clickJsCode_1);
    tableDirectLinks(jsRet_1.toStringList()[0]);

    // Get MD5 checksums
    const char* clickJsCode_2 = MULTI_LINE_STRING(
                function getMd5List()
                {
                    var stackLinks = [];
                    var allSpans = document.getElementsByTagName("code");
                    for (theSpan in allSpans)
                    {
                        var link = allSpans[theSpan].innerHTML;
                        if (link)
                        {
                            stackLinks.push(link);
                        }
                    }
                    return stackLinks;
                }
                getMd5List();
        );
    QVariant jsRet_2 = mainFrame->evaluateJavaScript(clickJsCode_2);
    md5ToTable(jsRet_2.toStringList()[0]);

    timer->stop();
    fileCNT++;

    if (fileCNT != stackSize) {
        ui->lineEdit->setText(ui->tableWidget->item(fileCNT + getArg(), 1)->text());
        goToURL();
    } else {
        directLink = false;
        flushToFile(getArg(), fileCNT + getArg());
        fileCNT = 0;
        if (pgtCNT < pageCount - 1) {
            pgtCNT++;
            ui->lineEdit->setText(QString("https://www.androidfilehost.com/?w=search&s=.xml.zip&type=files&page=%1").arg(pgtCNT));
            goToURL();
        } else if (pgtCNT == pageCount - 1) { // Last Page
            pgtCNT++;
            lastPage = true;
            ui->lineEdit->setText(QString("https://www.androidfilehost.com/?w=search&s=.xml.zip&type=files&page=%1").arg(pgtCNT));
            goToURL();
        } else { // All
            ui->label_2->setText("All Done!");
        }
    }
}
