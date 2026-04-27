import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import { viteSingleFile } from 'vite-plugin-singlefile';
import { compression } from 'vite-plugin-compression2';

export default defineConfig({
  plugins: [
    vue(),
    viteSingleFile(),
    compression({
      algorithm: 'gzip',
      include: /\.html$/,
      deleteOriginalFile: true
    })
  ],
  build: {
    outDir: '../data',
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
