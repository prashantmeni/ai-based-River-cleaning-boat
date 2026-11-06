export type WsStatus = "idle" | "connecting" | "open" | "closed" | "error";

export interface WebSocketClientHandlers {
  onMessage: (data: string) => void;
  onStatusChange: (status: WsStatus) => void;
}

export interface WebSocketClientOptions extends WebSocketClientHandlers {
  autoReconnect?: boolean;
  maxBackoffMs?: number;
}

/**
 * Thin wrapper over browser WebSocket: auto-reconnect with capped backoff,
 * strict string-only frames (the firmware only sends JSON text).
 */
export class WebSocketClient {
  private ws: WebSocket | null = null;
  private url: string;
  private attempts = 0;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private closedByUser = false;
  private status: WsStatus = "idle";
  private readonly opts: Required<WebSocketClientOptions>;

  constructor(url: string, handlers: WebSocketClientOptions) {
    this.url = url;
    this.opts = {
      autoReconnect: handlers.autoReconnect ?? true,
      maxBackoffMs: handlers.maxBackoffMs ?? 8000,
      onMessage: handlers.onMessage,
      onStatusChange: handlers.onStatusChange,
    };
  }

  getStatus(): WsStatus {
    return this.status;
  }

  getUrl(): string {
    return this.url;
  }

  setUrl(url: string): void {
    if (url === this.url) return;
    this.url = url;
    if (this.ws) {
      // Reconnect to the new endpoint immediately.
      this.closedByUser = false;
      this.teardownSocket();
      this.connect();
    }
  }

  connect(): void {
    if (
      this.ws &&
      (this.ws.readyState === WebSocket.OPEN ||
        this.ws.readyState === WebSocket.CONNECTING)
    ) {
      return;
    }
    this.closedByUser = false;
    this.openSocket();
  }

  disconnect(): void {
    this.closedByUser = true;
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    this.teardownSocket();
    this.setStatus("idle");
  }

  /** Returns false when the socket cannot accept data right now. */
  send(text: string): boolean {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(text);
      return true;
    }
    return false;
  }

  private openSocket(): void {
    this.setStatus("connecting");
    let ws: WebSocket;
    try {
      ws = new WebSocket(this.url);
    } catch {
      this.setStatus("error");
      this.scheduleReconnect();
      return;
    }
    this.ws = ws;

    ws.onopen = () => {
      this.attempts = 0;
      this.setStatus("open");
    };
    ws.onmessage = (ev: MessageEvent) => {
      if (typeof ev.data === "string") this.opts.onMessage(ev.data);
    };
    ws.onerror = () => {
      this.setStatus("error");
    };
    ws.onclose = () => {
      if (this.ws === ws) this.ws = null;
      this.setStatus("closed");
      if (!this.closedByUser && this.opts.autoReconnect) {
        this.scheduleReconnect();
      }
    };
  }

  private teardownSocket(): void {
    if (!this.ws) return;
    const ws = this.ws;
    this.ws = null;
    ws.onopen = null;
    ws.onmessage = null;
    ws.onerror = null;
    ws.onclose = null;
    try {
      ws.close();
    } catch {
      /* already closed */
    }
  }

  private scheduleReconnect(): void {
    if (this.reconnectTimer) return;
    const backoff = Math.min(
      500 * 2 ** this.attempts,
      this.opts.maxBackoffMs,
    );
    this.attempts += 1;
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      if (!this.closedByUser && this.opts.autoReconnect) this.openSocket();
    }, backoff);
  }

  private setStatus(status: WsStatus): void {
    if (this.status === status) return;
    this.status = status;
    this.opts.onStatusChange(status);
  }
}
