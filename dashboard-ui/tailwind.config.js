/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        bg: "#0B1220",
        sidebar: "#101827",
        card: "#151F30",
        elevated: "#1B283B",
        accent: "#19C7A1",
        secondary: "#38BDF8",
        warning: "#F5B942",
        critical: "#F05252",
        "t-primary": "#F1F5F9",
        "t-secondary": "#94A3B8",
        "t-border": "#293548",
      },
      fontFamily: {
        sans: ["Inter", "Segoe UI", "system-ui", "sans-serif"],
        mono: ["JetBrains Mono", "Cascadia Code", "Consolas", "monospace"],
      },
      boxShadow: {
        panel: "0 1px 2px rgba(0,0,0,0.35)",
      },
    },
  },
  plugins: [],
};
