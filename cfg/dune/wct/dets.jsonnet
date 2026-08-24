// cfg/dune/wct/dets.jsonnet
//
// Dynamic import map for DUNE detector descriptions.
//
// Each value is a function(params) -> DetectorDescription.  Callers select a
// detector by name and call the function with their params object:
//
//   local dets = import "dune/wct/dets.jsonnet";
//   local det  = dets[params.detname](params);
//
// Canonical detector names:
//   "pdhd"  --  ProtoDUNE Horizontal Drift (ProtoDUNE-HD)
//   "pdvd"  --  ProtoDUNE Vertical Drift   (ProtoDUNE-VD)
//
// Both detector functions accept an identical params argument signature; see
// the individual detector.jsonnet files for supported override fields.

{
    pdhd: import "dets/pdhd/detector.jsonnet",
    pdsp: import "dets/pdsp/detector.jsonnet",
    pdvd: import "dets/pdvd/detector.jsonnet",
}
