#ifndef MERMAIDDIALOGBASE_H
#define MERMAIDDIALOGBASE_H

#include <QDialog>

class QWebEngineView;
class QTimer;
class QVBoxLayout;

class MermaidDialogBase : public QDialog
{
    Q_OBJECT

public:
    explicit MermaidDialogBase(const QString &title, const QString &themeCss,
                               QWidget *parent = nullptr);
    ~MermaidDialogBase() override;
    QString generatedDiagram() const;

protected:
    virtual QString buildDiagram() const = 0;

    QString mermaidTheme() const;
    static QString mermaidPreviewHtml(const QString &escaped, const QString &theme);
    void setupMainLayout(QWidget *leftPanel, QVBoxLayout *leftLayout,
                         const QList<int> &sizes = {350, 550});

    QWebEngineView *m_preview;
    QTimer *m_previewTimer;

public slots:
    void schedulePreviewUpdate();

protected slots:
    void updatePreview();

private:
    QString m_mermaidTheme;
};

#endif
