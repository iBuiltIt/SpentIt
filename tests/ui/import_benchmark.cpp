#include "import_model.hpp"
#include <spentit/storage_document_gateway.hpp>
#include <spentit/import_view.hpp>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QSettings>
#include <QTemporaryDir>
#include <QFile>
#include <QTimer>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFont>
#include <algorithm>
#include <iostream>
#include <map>
#include <chrono>
#include <sys/resource.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif
using namespace spentit;
using Clock=std::chrono::steady_clock;
static double elapsed(Clock::time_point t) {return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
static void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
// Exercise the production gateway directly: a partial decorator can silently
// fall back to obsolete default repository capabilities.
static double peakMiB() {
    rusage u{};require(getrusage(RUSAGE_SELF,&u)==0,"getrusage failed");
#ifdef __APPLE__
    return static_cast<double>(u.ru_maxrss)/1048576.;
#else
    return static_cast<double>(u.ru_maxrss)/1024.;
#endif
}
static double residentMiB() {
#ifdef __APPLE__
    mach_task_basic_info info{};mach_msg_type_number_t count=MACH_TASK_BASIC_INFO_COUNT;
    require(task_info(mach_task_self(),MACH_TASK_BASIC_INFO,reinterpret_cast<task_info_t>(&info),&count)==KERN_SUCCESS,"task_info failed");
    return static_cast<double>(info.resident_size)/1048576.;
#else
    return -1;
#endif
}
int main(int argc,char** argv) {
    const auto startup=Clock::now();
    QGuiApplication app(argc,argv);app.setFont(QFont("Helvetica Neue",13));QQuickStyle::setStyle("Basic");
    if(qEnvironmentVariableIsSet("SPENTIT_BENCHMARK_DIAGNOSTICS"))require(diagnostics::Collector::instance().enable(),"Diagnostics unsupported");
    try {
        const int count=argc>1?std::stoi(argv[1]):100000;
        const bool groups=argc==3 && std::string_view(argv[2])=="--groups";
        const bool interactionsOnly=argc==3 && std::string_view(argv[2])=="--interactions";
        require(argc<=3 && (argc<3 || interactionsOnly || groups),"Optional mode is --interactions or --groups");
        require(count>0 && count<=200000 && (groups?count%6==0:count%10==0),"Use a multiple of 10 (six for groups) up to 200000");
        QTemporaryDir dir;require(dir.isValid(),"Temporary directory failed");
        QCoreApplication::setOrganizationName("SpentItBenchmark");QCoreApplication::setApplicationName("DurableUi");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,dir.path());
        ImportModel model(storage::document_gateway());
        QString previousPhase;
        if(qEnvironmentVariableIsSet("SPENTIT_BENCHMARK_PHASES"))
            QObject::connect(&model,&ImportModel::changed,&app,[&]{
                if(model.phase()!=previousPhase){previousPhase=model.phase();if(!previousPhase.isEmpty())std::cout<<"progress_ms="<<elapsed(startup)<<" phase="<<previousPhase.toStdString()<<std::endl;}
            });
        QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("workflow",&model);engine.load(QUrl("qrc:/spentit/ImportMain.qml"));
        require(!engine.rootObjects().isEmpty(),"QML load failed");
        auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());require(window,"No window");
        auto* ledger=window->findChild<QQuickItem*>("durableLedger");require(ledger,"No ledger");
        std::cout<<"phase=application_startup ms="<<elapsed(startup)<<std::endl;
        std::cout<<"rows="<<count<<" mode="<<(groups?"groups":interactionsOnly?"interactions":"full")<<" platform="<<QGuiApplication::platformName().toStdString()<<" qt="<<qVersion()<<std::endl;
        // Timer gaps measure UI-thread scheduling, not display frame latency.
        QElapsedTimer clock;clock.start();qint64 previous=clock.nsecsElapsed();double maxGap=0;std::size_t ticks=0;
        QTimer heartbeat;heartbeat.setTimerType(Qt::PreciseTimer);heartbeat.setInterval(5);
        QObject::connect(&heartbeat,&QTimer::timeout,[&]{const auto now=clock.nsecsElapsed();maxGap=std::max(maxGap,static_cast<double>(now-previous)/1e6);previous=now;++ticks;});heartbeat.start();
        auto settle=[&](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
        auto command=[&](const char* phase,const QString& action,const QVariantMap& args=QVariantMap{}) {
            maxGap=0;ticks=0;previous=clock.nsecsElapsed();const auto start=Clock::now();
            QEventLoop loop;bool success=false;bool finished=false;
            auto connection=QObject::connect(&model,&ImportModel::completed,&loop,[&](const QString&,bool ok){success=ok;finished=true;loop.quit();});
            require(!model.busy(),"Benchmark command admission is busy");
            model.command(action,args);const auto expectedOperation=diagnostics::Collector::instance().snapshot().active.id;
            if(!finished)loop.exec();QObject::disconnect(connection);
            const auto duration=elapsed(start);double interactionReadyMs=-1;
            if(!success || !model.error().isEmpty())throw std::runtime_error("Benchmark command failed: "+action.toStdString()+": "+model.error().toStdString());
            if(model.diagnosticsEnabled()) {
                const auto observation=diagnostics::Collector::instance().snapshot();
                require(observation.latest.id==expectedOperation && observation.latest.delivered!=0,"Wrong diagnostic operation completed");
                if(action=="map_import") {
                    for(const auto code:{diagnostics::Code::import_capture,diagnostics::Code::import_prepare,diagnostics::Code::import_populate,diagnostics::Code::import_resolve_transfers,diagnostics::Code::import_generate_transfers}) {
                        const auto& metric=observation.metrics[static_cast<unsigned>(code)];
                        require(metric.calls>0 && metric.running==0 && metric.failed==0,"Import phase timing missing or unfinished");
                        std::cout<<"import_span="<<diagnostics::name(code)<<" calls="<<metric.calls<<" inclusive_us="<<metric.inclusive<<" exclusive_us="<<metric.exclusive<<std::endl;
                    }
                }
                QCoreApplication::processEvents();QMetaObject::invokeMethod(window,"publishDiagnostics");
                if(!diagnostics::Collector::instance().snapshot().ui.interaction_ready){
                    QEventLoop readyLoop;QTimer poll,deadline;poll.setInterval(100);deadline.setSingleShot(true);
                    QObject::connect(&poll,&QTimer::timeout,&readyLoop,[&]{const auto current=diagnostics::Collector::instance().snapshot();if(current.view!=observation.view || current.ui.interaction_ready)readyLoop.quit();});
                    QObject::connect(&deadline,&QTimer::timeout,&readyLoop,&QEventLoop::quit);poll.start();deadline.start(10000);readyLoop.exec();
                }
                const auto settled=diagnostics::Collector::instance().snapshot();
                if(settled.view==observation.view){require(settled.ui.generation==observation.view && settled.ui.interaction_ready,"UI interaction readiness not established");interactionReadyMs=elapsed(start);}
                else std::cout<<"diagnostic_phase="<<phase<<" requested_generation="<<observation.view<<" replacement_generation="<<settled.view<<" readiness=superseded"<<std::endl;
            }
            QCoreApplication::processEvents(); // Deliver pending GUI work; no fixed command-settle sleep.
            std::cout<<"phase="<<phase<<" ms="<<duration<<" interaction_ready_ms="<<interactionReadyMs<<" ui_gap_ms="<<maxGap<<" ticks="<<ticks<<" rss_mib="<<residentMiB()<<" peak_mib="<<peakMiB();
            std::cout<<std::endl;
        };
        const auto path=dir.path()+"/synthetic.spentit";
        command("create","create",{{"path",path},{"protected",true},{"password","synthetic-benchmark-password"},{"confirmation","synthetic-benchmark-password"},{"name","Synthetic benchmark"},{"currency","CAD"}});
        if(groups) {
            require(count>=120,"Group workload needs at least 120 rows");
            QFile file(dir.path()+"/synthetic.qif");require(file.open(QIODevice::WriteOnly),"Fixture open failed");
            QVariantList mappings;
            const int perAccount=count/6;
            for(int account=0;account<6;++account) {
                QByteArray bytes="!Account\nNSynthetic "+QByteArray::number(account)+"\nTBank\n^\n!Type:Bank\n";
                for(int row=0;row<perAccount;++row) {
                    // Two genuinely repeated transfers pair one-to-one; other pairs
                    // have unique amounts. Exercise bracketed and alias references.
                    const bool transfer=row<perAccount/10;
                    const int units=transfer?(row<2?10:row+10):7;
                    bytes+="D2026-02-01\nT"+QByteArray(account%2==0?"-":"")+QByteArray::number(units)+"\nPSynthetic merchant\nL";
                    bytes+=transfer?(account%2==0?"[Synthetic "+QByteArray::number(account^1)+"]":"Synthetic "+QByteArray::number(account^1)):"General";
                    bytes+="\n^\n";
                }
                require(file.write(bytes)==bytes.size(),"Fixture write failed");
                mappings.push_back(QVariantMap{{"source",account},{"create",true},{"currency","CAD"},{"date","2026-01-01"}});
            }
            file.close();
            command("group_scan","import",{{"path",file.fileName()},{"profile","DMY"}});
            command("group_prepare","map_import",{{"mappings",mappings}});
            require(model.state().value("importGroup").toMap().value("unresolvedTransfers").toInt()==0,"Repeated transfers were not resolved");
            const auto group=model.state().value("importGroup").toMap().value("id");
            command("close_group","close");
            command("open_group","unlock",{{"path",path},{"password","synthetic-benchmark-password"}});
            for(int pass=0;pass<2;++pass)for(int account=0;account<6;++account) {
                command(("switch_"+std::to_string(pass)+"_"+std::to_string(account)).c_str(),"select_account",{{"id",QString::number(account+2)}});
                require(model.rowCount()==perAccount+1,"Group occurrence count changed");
                require(model.state().value("committed").toString()=="0.00","Draft changed committed balance");
                const auto transfers=perAccount/10;
                require(model.state().value("transfers").toList().size()==transfers,"Transfer occurrence count changed");
                const auto units=static_cast<std::int64_t>(transfers)*(transfers-1)/2+10*transfers-1+7*(perAccount-transfers);
                const auto expected=app::decimal(Money((account%2==0?-units:units)*100,Currency("CAD")));
                require(model.state().value("projected").toString().toStdString()==expected,"Group exact projection changed");
                require(model.state().value("importGroup").toMap().value("unresolvedTransfers").toInt()==0,"Group status changed");
            }
            const auto batch=model.state().value("pending").toMap().value("id");
            command("inspect_group_edit","inspect",{{"batch",batch},{"ordinal",perAccount-1}});
            const auto original=model.state().value("selected").toMap().value("source");
            auto edit=model.state().value("selected").toMap().value("draft").toMap();edit["payee"]="Edited synthetic merchant";
            command("save_group_edit","save",edit);
            require(model.state().value("selected").toMap().value("source")==original,"Group edit changed source evidence");
            require(model.state().value("importGroup").toMap().value("unresolvedTransfers").toInt()==0,"Ordinary edit changed transfer resolution");
            command("resolve_existing","group_resolve_transfers",{{"group",group}});
            require(model.state().value("importGroup").toMap().value("unresolvedTransfers").toInt()==0,"Repeated matching behavior changed");
            command("abort_group","group_abort",{{"group",group}});
            command("final_close","close");
            std::cout<<"correctness=passed scope=group-prepare-open-switch-resolve-abort"<<std::endl;return 0;
        }
        command("opening","opening",{{"date","2026-01-01"},{"amount","100.00"}});
        {
            QFile file(dir.path()+"/synthetic.qif");require(file.open(QIODevice::WriteOnly),"Fixture open failed");
            for(int i=0;i<count;++i) {
                QByteArray row="D"+QByteArray::number(i%28+1)+"/2/2026\nT"+(i%2==0?QByteArray("-10"):QByteArray("20"))+"\nP"+(i%10==0?QByteArray("Target "):QByteArray("Merchant "))+QByteArray::number(i)+"\nN"+QByteArray::number(i)+"\n";
                if(i%10==0)row+="SFood\nEMixed debit\n$-12\nSRefund\nEMixed credit\n$2\n";else row+="LGeneral\n";
                row+="^\n";require(file.write(row)==row.size(),"Fixture write failed");
            }
        }
        command("import","import",{{"path",dir.path()+"/synthetic.qif"},{"profile","DMY"},{"account",model.state().value("accountId")}});
        const Currency cad("CAD");const auto expected=app::decimal(Money(10000+static_cast<std::int64_t>(count/2)*1000,cad));
        require(model.state().value("projected").toString().toStdString()==expected,"Projected reconciliation failed");
        require(model.state().value("committed").toString()=="100.00" && model.rowCount()==count+1,"Draft isolation/count failed");
        const auto batch=model.state().value("selected").toMap().value("batch");
        command("close_pending","close");
        command("reopen_pending","unlock",{{"path",path},{"password","synthetic-benchmark-password"}});
        for(int column=0;column<6;++column)command(("sort_"+std::to_string(column)).c_str(),"query",{{"projected",true},{"sort",column},{"descending",true}});
        command("filter_target","query",{{"projected",true},{"filter","Target"}});
        require(model.rowCount()==count/10,"Filter count failed");
        require(model.state().value("withdrawals").toString().toStdString()==app::decimal(Money(-static_cast<std::int64_t>(count/10)*1000,cad)),"Filter sum failed");
        command("filter_empty","query",{{"projected",true},{"filter","absent-synthetic-match"}});require(model.rowCount()==0,"Empty filter failed");
        command("filter_clear","query",{{"projected",true}});
        command("inspect_last","inspect",{{"batch",batch},{"ordinal",count-1}});
        command("inspect_first","inspect",{{"batch",batch},{"ordinal",0}});
        const auto original=model.state().value("selected").toMap().value("source");
        auto* payee=window->findChild<QQuickItem*>("durablePayee");require(payee,"No editor");payee->setProperty("text","Unsaved benchmark edit");require(QMetaObject::invokeMethod(payee,"textEdited"),"Edit signal failed");
        if(QGuiApplication::platformName()!="offscreen"){window->raise();window->requestActivate();}
        settle(250);
        require(window->isExposed(),"Native window is not exposed; scrolling cannot be measured");
        // Compare the visible-only cache against this Qt build's actual default
        // on the same loaded dataset. No reset occurs during either traversal.
        QQmlComponent defaults(&engine);defaults.setData("import QtQuick\nListView {}",QUrl());
        std::unique_ptr<QObject> defaultList(defaults.create());require(defaultList!=nullptr,"Default ListView creation failed");
        const auto defaultCache=defaultList->property("cacheBuffer");
        for(bool cached:{false,true}) {
            ledger->setProperty("cacheBuffer",cached?defaultCache:QVariant(0));
            maxGap=0;previous=clock.nsecsElapsed();std::vector<double> frames;auto last=Clock::now();
            auto frameConnection=QObject::connect(window,&QQuickWindow::frameSwapped,window,[&]{frames.push_back(elapsed(last));last=Clock::now();},Qt::QueuedConnection);
            QTimer scroll;scroll.setInterval(16);scroll.setTimerType(Qt::PreciseTimer);int steps=0;QEventLoop scrolling;
            ledger->setProperty("contentY",0);settle(100);frames.clear();last=Clock::now();
            QObject::connect(&scroll,&QTimer::timeout,[&]{ledger->setProperty("contentY",steps*120);window->update();if(++steps==240)scrolling.quit();});scroll.start();scrolling.exec();scroll.stop();QObject::disconnect(frameConnection);
            require(payee->property("text").toString()=="Unsaved benchmark edit" && window->property("dirty").toBool(),"Recycled edit lost");
            require(!frames.empty() || QGuiApplication::platformName()=="offscreen", "No native frames observed; scrolling measurement unavailable");
            std::sort(frames.begin(),frames.end());
            std::cout<<"phase="<<(cached?"scroll_default_cache":"scroll")<<" cache_buffer="<<ledger->property("cacheBuffer").toInt()<<" steps="<<steps<<" frames="<<frames.size()<<" frame_p95_ms="<<(frames.empty()?0:frames[frames.size()*95/100])<<" frame_max_ms="<<(frames.empty()?0:frames.back())<<" ui_gap_ms="<<maxGap<<" rss_mib="<<residentMiB()<<" peak_mib="<<peakMiB()<<std::endl;
        }
        ledger->setProperty("cacheBuffer",0);
        require(QMetaObject::invokeMethod(window,"loadEditor"),"Discard editor failed");
        auto edit=model.state().value("selected").toMap().value("draft").toMap();edit["total"]="-11.00";edit["payee"]="Edited target";
        auto splits=edit.value("splits").toList();auto first=splits[0].toMap();first["amount"]="-13.00";splits[0]=first;edit["splits"]=splits;
        command("save","save",edit);
        require(model.state().value("selected").toMap().value("source")==original,"Original source changed");
        const auto corrected=app::decimal(Money(9900+static_cast<std::int64_t>(count/2)*1000,cad));
        require(model.state().value("projected").toString().toStdString()==corrected,"Save reconciliation failed");
        if(interactionsOnly){command("final_close","close");std::cout<<"correctness=passed scope=import-verified-reopen-edit-search-scroll"<<std::endl;return 0;}
        command("close_saved","close");command("reopen_saved","unlock",{{"path",path},{"password","synthetic-benchmark-password"}});
        command("inspect_saved","inspect",{{"batch",batch},{"ordinal",0}});
        require(model.state().value("selected").toMap().value("draft").toMap().value("payee").toString()=="Edited target","Saved edit lost");
        require(model.state().value("selected").toMap().value("source")==original,"Reopen evidence changed");
        require(model.state().value("newTransactions").toInt()==count,"Pending acceptance count failed");
        command("commit","commit");require(model.state().value("committed").toString().toStdString()==corrected,"Commit reconciliation failed");
        require(model.state().value("newTransactions").toInt()==0 && !model.state().value("receipt").toMap().value("operation").toString().isEmpty(),"Dataset acceptance failed");
        command("close_committed","close");command("reopen_committed","unlock",{{"path",path},{"password","synthetic-benchmark-password"}});
        require(model.state().value("committed").toString().toStdString()==corrected,"Reopen commit mismatch");
        command("final_close","close");std::cout<<"correctness=passed"<<std::endl;
    } catch(const std::exception& e){std::cerr<<"Benchmark failed: "<<e.what()<<'\n';return 1;}
}
