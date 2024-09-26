import { AppRegistry } from 'react-native';
import App from './src/App';
import { name as appName } from './app.json';
import { ALPRProvider } from 'vision-camera-plugin-anpr';

const Root = () => (
  <ALPRProvider config={{ country: 'eu', topN: 10, region: 'za' }}>
    <App />
  </ALPRProvider>
);

AppRegistry.registerComponent(appName, () => Root);
