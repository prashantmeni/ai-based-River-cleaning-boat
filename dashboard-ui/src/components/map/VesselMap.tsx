import { useEffect, useRef, useState } from "react";
import L from "leaflet";
import { MapPin, WifiOff } from "lucide-react";

export interface MapPos {
  lat: number;
  lon: number;
}

interface VesselMapProps {
  position: MapPos | null;
  trail?: MapPos[];
  height?: number;
}

/**
 * Plain Leaflet (no react-leaflet). Uses OSM tiles; when the laptop runs on
 * the boat AP WITHOUT internet the tiles fail — an honest offline notice is
 * shown instead of a fake map.
 */
export function VesselMap({ position, trail = [], height = 320 }: VesselMapProps) {
  const divRef = useRef<HTMLDivElement | null>(null);
  const mapRef = useRef<L.Map | null>(null);
  const markerRef = useRef<L.Marker | null>(null);
  const trailRef = useRef<L.Polyline | null>(null);
  const [tilesFailed, setTilesFailed] = useState(false);

  useEffect(() => {
    if (!divRef.current || mapRef.current) return;
    try {
      const map = L.map(divRef.current, {
        center: [17.385, 78.4867],
        zoom: 16,
        attributionControl: true,
      });
      const tiles = L.tileLayer(
        "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
        {
          maxZoom: 19,
          attribution: "&copy; OpenStreetMap contributors",
        },
      );
      tiles.on("tileerror", () => setTilesFailed(true));
      tiles.on("tileload", () => setTilesFailed(false));
      tiles.addTo(map);
      mapRef.current = map;
    } catch {
      setTilesFailed(true);
    }
    return () => {
      mapRef.current?.remove();
      mapRef.current = null;
      markerRef.current = null;
      trailRef.current = null;
    };
  }, []);

  useEffect(() => {
    const map = mapRef.current;
    if (!map) return;
    if (position) {
      const ll: L.LatLngExpression = [position.lat, position.lon];
      if (!markerRef.current) {
        markerRef.current = L.marker(ll, {
          title: "Boat position",
          icon: L.divIcon({
            className: "",
            html: '<div style="width:14px;height:14px;border-radius:999px;background:#19C7A1;border:3px solid #0B1220;box-shadow:0 0 8px #19C7A199"></div>',
            iconSize: [14, 14],
            iconAnchor: [7, 7],
          }),
        }).addTo(map);
      } else {
        markerRef.current.setLatLng(ll);
      }
      map.panTo(ll, { animate: true, duration: 0.5 });
    }
    const pts = trail
      .filter((p) => p.lat !== 0 || p.lon !== 0)
      .map((p) => [p.lat, p.lon] as L.LatLngExpression);
    if (pts.length >= 2) {
      if (trailRef.current) trailRef.current.setLatLngs(pts);
      else
        trailRef.current = L.polyline(pts, {
          color: "#38BDF8",
          weight: 2,
          opacity: 0.7,
          dashArray: "6 6",
        }).addTo(map);
    }
  }, [position, trail]);

  return (
    <div className="relative overflow-hidden rounded-lg border border-t-border" style={{ height }}>
      <div ref={divRef} className="h-full w-full" aria-label="Vessel map" />
      {tilesFailed && (
        <div className="absolute inset-0 flex flex-col items-center justify-center gap-2 bg-bg/85 p-4 text-center">
          <WifiOff className="h-6 w-6 text-warning" aria-hidden />
          <p className="text-sm font-medium text-t-primary">
            Map tiles unavailable offline
          </p>
          <p className="max-w-sm text-xs text-t-secondary">
            The laptop is connected to the boat Wi-Fi (no internet), so
            OpenStreetMap cannot download tiles. Coordinates below remain live;
            the map will fill in when internet is available.
          </p>
        </div>
      )}
      {!position && !tilesFailed && (
        <div className="absolute bottom-2 left-2 flex items-center gap-1.5 rounded bg-bg/85 px-2 py-1 text-xs text-warning">
          <MapPin className="h-3.5 w-3.5" /> Waiting for GPS fix
        </div>
      )}
    </div>
  );
}
