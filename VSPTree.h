#ifndef VSPTREE_H
#define VSPTREE_H

#include <QApplication>
#include <QtGui>
#include <QFile>
#include <QTextDocument>
#include <QObject>
#include <QMainWindow>
#include <QString>
#include <QSettings>
#include <QDomDocument>
#include <QThread>

class VSPTreeThread : public QThread
{
Q_OBJECT
public:
	QProcess m_process;
	QString m_fileName;
	QString m_profilerpath;
    void run();
signals:
	void output( QString output );
private:
	void log( QString input );
};

class VSPTree : public QMainWindow
{
    Q_OBJECT

public:
    VSPTree(QString appPath, QString argument);
	~VSPTree();

private:
	QTreeWidget* m_callTreeWidget;
	QTreeWidget* m_flatCallTreeWidget;
	QTreeWidget* m_functionSummaryTreeWidget;
	QMenu* m_callTreeMenu;
	QMenu* m_flatCallTreeMenu;
	QTextEdit* m_log;
	QProcess m_process;
	QTabWidget* m_tabWidget;
	QHash<QTreeWidgetItem*, QTreeWidgetItem*> m_flatHash;
	QString m_lastExe;
	QString m_lastVSP;
	QString m_lastArguments;
	QString m_applicationPath;
	QSettings m_settings;
	QString m_profilerpath;
	QString m_editorPath;
	QString m_editorArguments;
	VSPTreeThread m_thread;
	QMenuBar* menubar;

protected:
	void contextMenuEvent ( QContextMenuEvent * event );
	void closeEvent( QCloseEvent* e );
	int writeText(QString filePath,QString string);
	QDomDocument readXML(QString filepath);

protected slots:
	void LoadVSP(QString fileName);
	void LoadVSPClicked();
	void LoadCallTree(QString callTreeSummary);
	void LoadFunctionSummary(QString functionSummary);
	void GenerateReport(QString fileName);
	void Log(QString text);
	void StdOutLog();
	void StdErrorLog();
	void StopProfiling();
	void Profile();
	void StartProfiling();
	void FindInTree();
	void openFile();
	void setTextEditor();
	void workDone();
};


#endif
