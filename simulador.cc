/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <ns3/core-module.h>
#include <ns3/point-to-point-helper.h>
#include <ns3/data-rate.h>
#include <ns3/error-model.h>
#include <ns3/random-variable-stream.h>
#include <ns3/point-to-point-net-device.h>
#include "Enlace.h"
#include "Observador.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Practica04");


int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::US);

  // Parámetros de la simulación
  Time     trtx       = Time("17ms");
  uint32_t tamPaquete = 994;
  Time     rprop      = Time("4ms");
  DataRate vtx        = DataRate("1000kbps");
  uint8_t  tamVentana = 3;
  double   perror     = 0.00002;

  // Creamos el modelo de error y le asociamos los parametros
  Ptr<UniformRandomVariable> uniforme = CreateObject<UniformRandomVariable>();
  Ptr<RateErrorModel> modelo_error = CreateObject<RateErrorModel> ();

  modelo_error->SetRandomVariable(uniforme);
  modelo_error->SetRate(perror);
  modelo_error->SetUnit(RateErrorModel::ERROR_UNIT_BIT);

  // Configuramos el escenario:
  PointToPointHelper escenario;
  escenario.SetChannelAttribute ("Delay", TimeValue (rprop));
  escenario.SetDeviceAttribute ("DataRate", DataRateValue (vtx));
  escenario.SetQueue ("ns3::DropTailQueue");
  // Creamos los nodos
  NodeContainer      nodos;
  nodos.Create(2);
  // Creamos el escenario
  NetDeviceContainer dispositivos = escenario.Install (nodos);

  // Una aplicación transmisora (que tambien recibira)
  Enlace transmisor (dispositivos.Get (1), trtx, tamPaquete, tamVentana);
  // Y una receptora (que tambien enviara)
  Enlace receptor (dispositivos.Get (0), trtx, tamPaquete, tamVentana);

  dispositivos.Get(0)->GetObject<PointToPointNetDevice>()->SetReceiveErrorModel(modelo_error);
  dispositivos.Get(1)->GetObject<PointToPointNetDevice>()->SetReceiveErrorModel(modelo_error);

  Observador observador;
  // Suscribimos la traza de paquetes correctamente asentidos.
  dispositivos.Get (0)->TraceConnectWithoutContext ("MacRx", MakeCallback(&Observador::PaqueteAsentido, &observador));
  dispositivos.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback(&Observador::PaqueteErroneo, &observador));

  // Añadimos cada aplicación a su nodo
  nodos.Get (0)->AddApplication(&transmisor);
  nodos.Get (1)->AddApplication(&receptor);

  //Añadimos una salida a pcap
  escenario.EnablePcap("ppractica04", dispositivos.Get(0));

  // Activamos el transmisor
  transmisor.SetStartTime (Seconds (1.0));
  transmisor.SetStopTime (Seconds (9.95));
  // Activamos el transmisor
  receptor.SetStartTime (Seconds (1.0));
  receptor.SetStopTime (Seconds (9.95));

  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_DEBUG ("TamPaquete: " << tamPaquete + 8);
  NS_LOG_DEBUG ("Vtx: " << vtx);
  NS_LOG_DEBUG ("Rprop: " << rprop);
  NS_LOG_DEBUG ("RTT: " << Seconds(vtx.CalculateTxTime (tamPaquete + 8)) + 2 * rprop);
  NS_LOG_DEBUG ("Temporizador: " << trtx);
  NS_LOG_DEBUG ("Probabilidad de error de bit: " << perror);
  NS_LOG_INFO  ("Total paquetes: " << observador.TotalPaquetes ());
  NS_LOG_INFO  ("Total paquetes erroneos: " << observador.TotalPaquetesErroneos ());

  return 0;
}
