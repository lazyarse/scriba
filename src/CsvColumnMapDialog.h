#pragma once

#include <QDialog>
#include "CsvReader.h"

class QComboBox;
class QCheckBox;
class QPlainTextEdit;

class CsvColumnMapDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CsvColumnMapDialog(const QStringList &chartFields, QWidget *parent = nullptr);

    QHash<QString, int> mapping() const;
    CsvData csvData() const { return m_csvData; }

private slots:
    void showPasteDialog();
    void onOpenFile();
    void onHeadersToggled(bool checked);
    void onUpdate();

private:
    void buildUi();
    void reloadData(const QString &rawText);
    void populateCombos();

    QStringList m_chartFields;
    CsvData m_csvData;
    QString m_rawText;
    bool m_firstRowIsHeaders = true;

    QCheckBox *m_headersCheck;
    QWidget *m_mappingContainer;
    QList<QComboBox*> m_fieldCombos;
};
