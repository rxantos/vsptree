#include "VSPTree.h"

#define VERSION "VSPTree 0.4"
#define PATH32 "C:\\Program Files\\Microsoft Visual Studio 9.0\\Team Tools\\Performance Tools\\"
#define PATH64 "C:\\Program Files (x86)\\Microsoft Visual Studio 9.0\\Team Tools\\Performance Tools\\"

VSPTree::VSPTree( QString appPath,  QString argument) :
	m_applicationPath( appPath ),
	m_settings( appPath + "/VSPTree.ini", QSettings::IniFormat )
{ 
	QDir dir(PATH32);
	QDir dir2(PATH64);
	if ( dir.exists() )
	{
		m_profilerpath = PATH32;
	}
	else if ( dir2.exists() )
	{
		m_profilerpath = PATH64;
	}
	else
	{
		QMessageBox::critical(NULL,"Standalone Debugger not installed", "C:\\Program Files (x86)\\Microsoft Visual Studio 9.0\\Team Tools\\Performance Tools\\ does not exist");
	}

	QStringList env = QProcess::systemEnvironment();
	env.replaceInStrings(QRegExp("^PATH=(.*)", Qt::CaseInsensitive), QString("PATH=\\1;") + m_profilerpath);
	m_process.setEnvironment(env);

	if ( m_settings.contains("m_lastExe") ) 
	{
		m_lastExe = m_settings.value("m_lastExe").toString();
	}

	if ( m_settings.contains("m_lastVSP") ) 
	{
		m_lastVSP = m_settings.value("m_lastVSP").toString();
	}

	if ( m_settings.contains("m_lastArguments") ) 
	{
		m_lastArguments = m_settings.value("m_lastArguments").toString();
	}

	m_log = new QTextEdit();
	m_log->setReadOnly(true);
	connect (&m_process, SIGNAL(readyReadStandardOutput()),this,  SLOT(StdOutLog()));
	connect (&m_process, SIGNAL(readyReadStandardError()),this,  SLOT(StdErrorLog()));

	QMenuBar* menubar = this->menuBar();
	QMenu* fileMenu = menubar->addMenu("File");
	QAction* openAct = fileMenu->addAction("Open VSP");
	connect(openAct, SIGNAL(triggered()), this, SLOT(LoadVSPClicked()));
	QAction* exitAct = fileMenu->addAction("Exit");
	connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));
	
	QMenu* profilerMenu = menubar->addMenu("Profiler");

	QAction* startProfilingAct = profilerMenu->addAction("Start Profiler");
	connect(startProfilingAct, SIGNAL(triggered()), this, SLOT(StartProfiling()));

	QAction* stopProfilingAct = profilerMenu->addAction("Stop Profiler");
	connect(stopProfilingAct, SIGNAL(triggered()), this, SLOT(StopProfiling()));

	m_tabWidget = new QTabWidget();

	m_callTreeWidget = new QTreeWidget();

	m_flatCallTreeWidget = new QTreeWidget();

	m_functionSummaryTreeWidget = new QTreeWidget();

	m_tabWidget->addTab(m_callTreeWidget,"CallTree");
	m_tabWidget->addTab(m_flatCallTreeWidget,"CallTreeFlat");
	m_tabWidget->addTab(m_functionSummaryTreeWidget,"FunctionSummary");
	m_tabWidget->addTab(m_log,"Log");

	m_callTreeMenu = new QMenu();
	QAction* expandAllAct = m_callTreeMenu->addAction("Expand All");
	connect(expandAllAct, SIGNAL(triggered()), m_callTreeWidget, SLOT(expandAll()));

	QAction* collapseAllAct = m_callTreeMenu->addAction("Collapse All");
	connect(collapseAllAct, SIGNAL(triggered()), m_callTreeWidget, SLOT(collapseAll()));

	m_flatCallTreeMenu = new QMenu();
	QAction* FindInTreeAct = m_flatCallTreeMenu->addAction("Find In Tree");
	connect(FindInTreeAct, SIGNAL(triggered()), this, SLOT(FindInTree()));

	this->setCentralWidget(m_tabWidget);
	this->setWindowTitle(VERSION);
	this->resize(640,480);
	this->show();

	if ( !argument.isEmpty() )
	{
		LoadVSP(argument);
	}
}

VSPTree::~VSPTree()
{ 

}

void VSPTree::LoadVSPClicked()
{
	QString fileName = QFileDialog::getOpenFileName(0,"VSP to open",m_lastVSP,"*.vsp");
	if ( !fileName.isNull() )
	{
		LoadVSP(fileName);
	}
}

void VSPTree::LoadVSP(QString fileName)
{
	m_callTreeWidget->clear();
	m_flatCallTreeWidget->clear();
	m_functionSummaryTreeWidget->clear();

	GenerateReport(fileName);

	QFileInfo fileinfo(fileName);

	m_lastVSP = fileName;

	QString callTreeSummary = fileinfo.absolutePath() + "/" + fileinfo.baseName() + QString("_CallTreeSummary.xml");
	LoadCallTree(callTreeSummary);

	QString functionSummary = fileinfo.absolutePath() + "/" + fileinfo.baseName() + QString("_FunctionSummary.xml");
	LoadFunctionSummary(functionSummary);
}

void VSPTree::LoadCallTree(QString callTreeSummary)
{
	Log("Reading : " + callTreeSummary);
	QDomDocument doc = readXML( callTreeSummary );

	QTreeWidgetItem* root = new QTreeWidgetItem(m_callTreeWidget);
	root->setText(0,"CallTree");
	m_callTreeWidget->insertTopLevelItem(0, root);

	QTreeWidgetItem* flatroot = new QTreeWidgetItem(m_flatCallTreeWidget);
	flatroot->setText(0,"FlatCallTree");
	m_callTreeWidget->insertTopLevelItem(0, flatroot);

	QDomNodeList functionList = doc.elementsByTagName("CallTree");
	
	QHash<int, QTreeWidgetItem*> levelHash;

	levelHash.insert(0,root);

	QStringList fields;
	fields << "FunctionName" << "InclSamples" << "ExclSamples" << "InclSamplesPercent"
		<< "ExclSamplesPercent" << "SourceFile" << "LineNumber" << "ModuleName" << "ModulePath" 
		<< "FunctionAddress" << "Level" << "ProcessName" << "PID"; 

	
	m_callTreeWidget->setColumnCount(fields.count());
	m_callTreeWidget->setHeaderLabels(fields);
	
	m_flatCallTreeWidget->setColumnCount(fields.count());
	m_flatCallTreeWidget->setHeaderLabels(fields);

	for ( int i = 0; i < functionList.count(); i++ )
	{
		QTreeWidgetItem* tempWidgetItem = new QTreeWidgetItem();
		for ( int j = 0; j < fields.count(); j++ )
		{
			QString cellText = functionList.at(i).toElement().attribute( fields.at(j) );
			if ( fields.at(j) == "ExclSamplesPercent" && cellText.count() < 5)
			{
				cellText.prepend('0');
			}
			if ( fields.at(j) == "InclSamplesPercent" && cellText.count() < 5)
			{
				cellText.prepend('0');
			}
			if ( fields.at(j) == "Level" && cellText.count() < 2)
			{
				cellText.prepend('0');
			}
			tempWidgetItem->setText( j, cellText );
		}
		int level = functionList.at(i).toElement().attribute("Level").toInt();
		levelHash.value(level)->addChild(tempWidgetItem);
		QTreeWidgetItem* flatTreeWidgetItem = tempWidgetItem->clone();
		flatroot->addChild(flatTreeWidgetItem);
		m_flatHash.insert(flatTreeWidgetItem,tempWidgetItem);
		levelHash.insert(level+1,tempWidgetItem);
	}

	m_callTreeWidget->expandAll();
	m_callTreeWidget->setAlternatingRowColors(true);
	m_flatCallTreeWidget->expandAll();
	m_flatCallTreeWidget->setAlternatingRowColors(true);
	m_flatCallTreeWidget->setSortingEnabled(true);
}

void VSPTree::LoadFunctionSummary(QString functionSummary)
{
	Log("Reading : " + functionSummary);
	QDomDocument doc = readXML( functionSummary );

	QTreeWidgetItem* root = new QTreeWidgetItem(m_functionSummaryTreeWidget);
	root->setText(0,"FunctionSummary");
	m_functionSummaryTreeWidget->insertTopLevelItem(0, root);

	QDomNodeList functionList = doc.elementsByTagName("Function");

	QStringList fields;
	fields << "FunctionName" << "InclSamples" << "ExclSamples" << "InclSamplesPercent"
		<< "ExclSamplesPercent" << "SourceFile" << "LineNumber" << "ModuleName" << "ModulePath" 
		<< "FunctionAddress" << "ProcessName" << "PID"; 

	m_functionSummaryTreeWidget->setColumnCount(fields.count());
	m_functionSummaryTreeWidget->setHeaderLabels(fields);

	for ( int i = 0; i < functionList.count(); i++ )
	{
		QTreeWidgetItem* tempWidgetItem = new QTreeWidgetItem();
		for ( int j = 0; j < fields.count(); j++ )
		{
			QString cellText = functionList.at(i).toElement().attribute( fields.at(j) );
			if ( fields.at(j) == "ExclSamplesPercent" && cellText.count() < 5)
			{
				cellText.prepend('0');
			}
			if ( fields.at(j) == "InclSamplesPercent" && cellText.count() < 5)
			{
				cellText.prepend('0');
			}
			if ( fields.at(j) == "Level" && cellText.count() < 2)
			{
				cellText.prepend('0');
			}
			tempWidgetItem->setText( j, cellText );
		}
		root->addChild(tempWidgetItem);
	}

	m_functionSummaryTreeWidget->expandAll();
	m_functionSummaryTreeWidget->setAlternatingRowColors(true);
	m_functionSummaryTreeWidget->setSortingEnabled(true);

}

void VSPTree::contextMenuEvent ( QContextMenuEvent * event ) 
{
	if ( m_tabWidget->currentWidget() == m_callTreeWidget )
	{
		m_callTreeMenu->exec(QCursor::pos());
	}
	else if ( m_tabWidget->currentWidget() == m_flatCallTreeWidget ) 
	{
		m_flatCallTreeMenu->exec(QCursor::pos());
	}
	event->accept();
}

void VSPTree::GenerateReport(QString fileName)
{
	m_tabWidget->setCurrentWidget(m_log);
	QFileInfo fileInfo(fileName);
	m_process.setWorkingDirectory(fileInfo.absolutePath());
	m_process.start(m_profilerpath + QString("VSPerfReport.exe"), QStringList() << "/summary:all" << "/packsymbols" << "/XML" << fileName );
	m_process.waitForFinished(-1);
}

void VSPTree::StopProfiling()
{
	m_tabWidget->setCurrentWidget(m_log);
	m_process.start(m_profilerpath + QString("VsPerfCmd.exe"), QStringList() << "-shutdown" );
	m_process.waitForFinished();
}

void VSPTree::StartProfiling()
{
	QString fileName = QFileDialog::getOpenFileName(0,"Executable to Profile",m_lastExe,"*.exe");
	if ( fileName.isNull() ) 
	{
		return;
	}

	m_lastExe = fileName;

	bool ok;
	QString argumentstext = QInputDialog::getText(this, "Arguments", "Arguments to pass to executable. Leave blank or cancel for nothing.", QLineEdit::Normal, m_lastArguments, &ok);
	if (ok)
	{
		m_lastArguments = argumentstext;
	}
	else
	{
		return;
	}


	QString reportfileName = QFileDialog::getSaveFileName(0,"Report to save",m_lastVSP,"*.vsp");
	if ( !fileName.isNull() && !reportfileName.isNull() )
	{
		m_lastVSP = reportfileName;
		m_tabWidget->setCurrentWidget(m_log);
		QStringList args;
		args << "VsPerfCmd.exe" 
			<< "/start:sample" << QString("/output:\"") + reportfileName + QString("\"") 
			<< QString("/launch:\"") + fileName + QString("\"")
			<< QString("/ARGS:\"") + argumentstext + QString("\"")
			;
		Log(args.join(" "));
		writeText("StartProfiling.bat", 
			QString("SET PATH=%PATH%;") + m_profilerpath + QString("\n") + QString("VsPerfCmd.exe -shutdown\n") + args.join(" ")
			);
		m_process.startDetached("StartProfiling.bat",args);
	}
}


void VSPTree::Log(QString text)
{
	m_log->append(text);
}

void VSPTree::StdOutLog()
{
	Log("************ BEGIN *************");
	Log(QString(m_process.readAllStandardOutput().data()));
	Log("************* END **************");
}

void VSPTree::StdErrorLog()
{
	Log("************ BEGIN *************");
	Log(QString(m_process.readAllStandardError().data()));
	Log("************* END **************");
}

void VSPTree::FindInTree()
{
	m_callTreeWidget->setCurrentItem(m_flatHash.value(m_flatCallTreeWidget->currentItem()));
	m_tabWidget->setCurrentWidget(m_callTreeWidget);
}

void VSPTree::closeEvent( QCloseEvent* e )
{
	QSettings settings( m_applicationPath + "/VSPTree.ini", QSettings::IniFormat );
    settings.setValue( "m_lastExe", m_lastExe );
	settings.setValue( "m_lastVSP", m_lastVSP );
	settings.setValue( "m_lastArguments", m_lastArguments );
	e->accept();
}

int VSPTree::writeText(QString filePath,QString string){
	QFile fileout(filePath);
	if (!fileout.open(QIODevice::WriteOnly | QIODevice::Text)){
		Log("Error Cannot Open : " + fileout.fileName());
		return 1;
	}
	QTextStream out(&fileout);
	//out.setCodec("UTF-8");
	out << string;
	fileout.close();
	return 0;
}

QDomDocument VSPTree::readXML(QString filepath){
	QDomDocument doc("Option");
	QFile file(filepath);

	if( !file.open( QIODevice::ReadOnly ) ) {
		Log("Unable to Open " + filepath);
		doc.clear();
		return doc;
	}

	if( !doc.setContent( &file ) ) {
		file.close();
		Log(filepath + " is not well formed");
		doc.clear();
		return doc;
	}
	file.close();

	return doc;
}