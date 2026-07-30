#pragma once

#include <QDialog>

class QComboBox;
class QTableWidget;
class QWebEngineView;
class QGroupBox;
class QLineEdit;
class QCheckBox;
class QTimer;

class VegaLiteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VegaLiteDialog(QWidget *parent = nullptr);
    QString generatedSpec() const;
    QList<QMap<QString, QString>> parseCsvData(const QString &text) const;
    QList<QMap<QString, QString>> parseJsonData(const QString &text) const;

private slots:
    void onChartTypeChanged();
    void onDataChanged();
    void schedulePreviewUpdate();
    void updatePreview();
    void pasteCsv();
    void openCsv();
    void pasteJson();

private:
    void setupUi();
    void setupLeftPanel(QWidget *panel);
    void updateEncodingVisibility();
    void updateFieldComboBoxes();
    void populateTableFromCsvData(const struct CsvData &data);
    QString buildSpec() const;

    QComboBox *m_chartTypeCombo;
    QTableWidget *m_table;
    QComboBox *m_fieldX, *m_typeX;
    QComboBox *m_fieldY, *m_typeY;
    QComboBox *m_fieldColor, *m_typeColor;
    QComboBox *m_fieldSize, *m_typeSize;
    QComboBox *m_fieldShape;
    QComboBox *m_fieldText;
    QCheckBox *m_tooltipCheck;
    QLineEdit *m_titleEdit;
    QCheckBox *m_fillWidthCheck;
    QWebEngineView *m_preview;
    QGroupBox *m_colorGroup;
    QGroupBox *m_sizeGroup;
    QGroupBox *m_shapeGroup;
    QGroupBox *m_textGroup;
    QTimer *m_previewTimer;
};

