#include "LogWindow.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QIcon>
#include <QDateTime>
#include <QScrollBar>
#include <QLoggingCategory>
#include <QMessageLogContext>

static QtMessageHandler s_originalHandler = nullptr;
static LogWindow *s_logWindowInstance = nullptr;

static void logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (s_logWindowInstance) {
        QString src = QString::fromLatin1(context.category);
        if (src.startsWith("scriba."))
            src = src.mid(7);
        else if (src.startsWith("scriba"))
            src = "app";
        else
            src = src;

        LogWindow::Level lvl;
        switch (type) {
        case QtDebugMsg:      lvl = LogWindow::Info;    break;
        case QtWarningMsg:    lvl = LogWindow::Warning; break;
        case QtCriticalMsg:
        case QtFatalMsg:      lvl = LogWindow::Error;   break;
        default:              lvl = LogWindow::Info;    break;
        }

        s_logWindowInstance->addEntry(lvl, src, msg);
    }

    if (s_originalHandler)
        s_originalHandler(type, context, msg);
}

LogWindow::LogWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Debug Log");
    resize(640, 320);

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setStyleSheet("font-family: 'monospace'; font-size: 12pt;");
    layout->addWidget(m_output);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    QPushButton *clearBtn = buttons->addButton("C&lear", QDialogButtonBox::ActionRole);
    connect(clearBtn, &QPushButton::clicked, m_output, &QTextEdit::clear);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    for (auto *btn : buttons->buttons())
        btn->setIcon(QIcon());
    layout->addWidget(buttons);

    if (!s_originalHandler)
        s_originalHandler = qInstallMessageHandler(logMessageHandler);
    s_logWindowInstance = this;

    QLoggingCategory::setFilterRules("scriba.*=true");
}

LogWindow::~LogWindow()
{
    if (s_logWindowInstance == this)
        s_logWindowInstance = nullptr;
}

void LogWindow::addEntry(Level level, const QString &source, const QString &message)
{
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString levelTag, levelColor;
    switch (level) {
    case Info:    levelTag = "INFO";  levelColor = "#26a269"; break;
    case Warning: levelTag = "WARN";  levelColor = "#b58900"; break;
    case Error:   levelTag = "ERR ";  levelColor = "#c01c28"; break;
    }

    QString html = QString(
        "<div style='padding-bottom:4px'>"
        "<span style='color:#808080'>%1</span> "
        "<b style='color:%2'>[%3]</b> "
        "<span style='color:#2d7ee9'>%4</span> "
        "<span>%5</span><br><br></div>"
    ).arg(ts, levelColor, levelTag, source, message.toHtmlEscaped());

    m_output->moveCursor(QTextCursor::End);
    m_output->insertHtml(html);

    m_output->verticalScrollBar()->setValue(
        m_output->verticalScrollBar()->maximum());
}
