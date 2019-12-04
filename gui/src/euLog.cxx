#include "eudaq/OptionParser.hh"
#include "eudaq/LogCollector.hh"
#include <iostream>
#include <QApplication>

int main(int argc, char **argv) {
  QCoreApplication *qapp = new QApplication(argc, argv );  
  eudaq::OptionParser op("EUDAQ Log Collector", "2.0",  "A Qt version of the Log Collector");
  eudaq::Option<std::string> rctrl(op, "r", "runcontrol",  "tcp://localhost:44000", "address", "The address of the RunControl application");
  eudaq::Option<std::string> listen(op, "a", "listen-address", "", "address", "The address on which to listen for log connections");
  eudaq::Option<int>             x(op, "x", "left",   0, "pos");
  eudaq::Option<int>             y(op, "y", "top",    0, "pos");
  eudaq::Option<int>             w(op, "w", "width",  150, "pos");
  eudaq::Option<int>             h(op, "g", "height", 200, "pos", "The initial position of the window");
  try {
    op.Parse(argv);
  } catch (...) {
    std::ostringstream err;
    return op.HandleMainException(err);
  }

  auto app=eudaq::Factory<eudaq::LogCollector>::
    MakeShared<const std::string&, const std::string&>
    (eudaq::cstr2hash("GuiLogCollector"), "log", rctrl.Value());
  app->SetPosition(x.Value(), y.Value(), w.Value(), h.Value());
  app->Exec();
  
  return 0;
}
