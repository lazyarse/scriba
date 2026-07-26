#include "LogWindow.h"
#include "StaticHelpers.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QIcon>
#include <QDateTime>
#include <QScrollBar>
#include <QLoggingCategory>
#include <QMessageLogContext>

static constexpr int kMaxEntries = 1000;

struct CachedEntry {
    LogWindow::Level level;
    QString source;
    QString message;
    QString timestamp;
};

static QtMessageHandler s_prevHandler = nullptr;
static LogWindow *s_logWindowInstance = nullptr;
static QVector<CachedEntry> s_buffer;

static void globalHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString category = QString::fromLatin1(context.category);
    bool isScriba = category.startsWith("scriba");

    LogWindow::Level lvl;
    switch (type) {
    case QtDebugMsg:      lvl = LogWindow::Info;    break;
    case QtWarningMsg:    lvl = LogWindow::Warning; break;
    case QtCriticalMsg:
    case QtFatalMsg:      lvl = LogWindow::Error;   break;
    default:              lvl = LogWindow::Info;    break;
    }

    QString src = category;
    if (src.startsWith("scriba."))
        src = src.mid(7);
    else if (src.startsWith("scriba"))
        src = "app";

    if (!isScriba && s_prevHandler) {
        s_prevHandler(type, context, msg);
        return;
    }

    if (s_logWindowInstance) {
        s_logWindowInstance->addEntry(lvl, src, msg);
    } else {
        QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
        if (s_buffer.size() >= kMaxEntries)
            s_buffer.removeFirst();
        s_buffer.append({lvl, src, msg, ts});
    }
}

void LogWindow::initDebugLogging()
{
    QLoggingCategory::setFilterRules("scriba.*=true");
    s_prevHandler = qInstallMessageHandler(globalHandler);
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
    buttons->button(QDialogButtonBox::Close)->setText(tr("&Close"));
    QPushButton *clearBtn = buttons->addButton("C&lear", QDialogButtonBox::ActionRole);
    connect(clearBtn, &QPushButton::clicked, m_output, &QTextEdit::clear);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    stripButtonIcons(buttons);
    layout->addWidget(buttons);

    s_logWindowInstance = this;

    for (auto &e : s_buffer)
        addEntry(e.level, e.source, e.message);
    s_buffer.clear();
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
