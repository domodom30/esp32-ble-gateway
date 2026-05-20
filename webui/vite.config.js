import { defineConfig } from 'vite';
import { rmSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import vue from '@vitejs/plugin-vue';
import { viteSingleFile } from 'vite-plugin-singlefile';
import { compression } from 'vite-plugin-compression2';

const __dirname = dirname(fileURLToPath(import.meta.url));

const OUT_DIR = '../data';

// Only the gzipped page is served by the ESP32 (handleHome -> /index.html.gz).
// Drop the uncompressed copy so it doesn't waste SPIFFS space.
function dropUncompressedHtml() {
  return {
    name: 'drop-uncompressed-html',
    closeBundle() {
      rmSync(resolve(__dirname, OUT_DIR, 'index.html'), { force: true });
    },
  };
}

export default defineConfig({
  plugins: [
    vue(),
    viteSingleFile(),
    compression({
      algorithm: 'gzip',
      include: /\.html$/,
      deleteOriginalFile: true
    }),
    dropUncompressedHtml()
  ],
  build: {
    outDir: OUT_DIR,
    emptyOutDir: false,
    minify: 'terser',
    terserOptions: {
      compress: {
        drop_console: true,
        passes: 2
      }
    }
  }
});
