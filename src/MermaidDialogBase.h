#pragma once

#include <QDialog>

class QCheckBox;
class QSpinBox;
class QTableWidget;
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
    QString mermaidBlock() const;

protected:
    virtual QString buildDiagram() const = 0;

    QString mermaidTheme() const;
    static QString mermaidPreviewHtml(const QString &escaped, const QString &theme,
                                      const QString &bgColor = QString());
    void setupMainLayout(QWidget *leftPanel, QVBoxLayout *leftLayout,
                         const QList<int> &sizes = {350, 550});

    QColor iconColor() const { return m_iconColor; }
    void addDeleteButton(QTableWidget *table, int column, int row);

    QWebEngineView *m_preview;
    QTimer *m_previewTimer;
    QCheckBox *m_widthCheck;
    QSpinBox *m_widthSpin;

public slots:
    void schedulePreviewUpdate();

protected slots:
    void updatePreview();

private:
    QString m_mermaidTheme;
    QString m_bgColor;
    QColor m_iconColor;
};

