import { PermissionsAndroid, Platform, Alert } from 'react-native';
import { NavigationContainer } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { HomeScreen } from './screens/HomeScreen';
import { RealTimeScanScreen } from './screens/RealTimeScanScreen';
import { RealTimeRecogniseScreen } from './screens/RealTimeRecogniseScreen';
import { ChooseFileScreen } from './screens/ChooseFileScreen';
import type { RootStackParamList } from './types/navigation';
import { useEffect } from 'react';
import requestPermissions from './services/request-permission.service';

const Stack = createNativeStackNavigator<RootStackParamList>();

export default function App() {
  useEffect(() => {
    requestPermissions();
  }, []);

  return (
    <NavigationContainer>
      <Stack.Navigator screenOptions={{ headerShown: false }}>
        <Stack.Screen name="Home" component={HomeScreen} />
        <Stack.Screen name="RealTimeScan" component={RealTimeScanScreen} />
        <Stack.Screen
          name="RealTimeRecognise"
          component={RealTimeRecogniseScreen}
        />
        <Stack.Screen name="ChooseFile" component={ChooseFileScreen} />
      </Stack.Navigator>
    </NavigationContainer>
  );
}
