import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

export default defineConfig({
	plugins: [sveltekit()],
	server: {
		proxy: {
			// Proxy all requests starting with `/api` to your backend server
			'/api': {
				target: 'http://localhost:9000', // Your backend server URL
				changeOrigin: true,
				//rewrite: (path) => path.replace(/^\/api/, ''), // Remove `/api` prefix when forwarding
			},
		},
	},
});
