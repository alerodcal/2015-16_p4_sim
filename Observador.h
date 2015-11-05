/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <ns3/packet.h>

using namespace ns3;


class Observador
{
public:
  Observador  ();
  void     PaqueteAsentido (Ptr<const Packet> paquete);
  uint64_t TotalPaquetes   ();
  void     PaqueteErroneo (Ptr<const Packet> paquete);
  uint64_t TotalPaquetesErroneos   ();

private:
  uint64_t m_paquetes;
  uint64_t m_paquetesError;
};
