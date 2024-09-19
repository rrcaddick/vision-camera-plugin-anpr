import { StyleSheet, View, Text } from 'react-native';
import { useALPR } from 'vision-camera-plugin-anpr';
import {
  Camera,
  useCameraPermission,
  useCameraDevice,
  useFrameProcessor,
  runAsync,
} from 'react-native-vision-camera';
import { useEffect } from 'react';

export default function App() {
  const { hasPermission, requestPermission } = useCameraPermission();
  const device = useCameraDevice('back');
  const {
    recogniseFrame,
    setTopN,
    setCountry,
    setDefaultRegion,
    setDetectRegion,
    setPrewarp,
  } = useALPR();

  useEffect(() => {
    if (!hasPermission) {
      requestPermission();
    }
    setTopN(5);
    setCountry('eu');
    setDefaultRegion('za');
    setDetectRegion(false);
    setPrewarp('planar,1280,720,0.8,0,0,0,0');
  }, [hasPermission, requestPermission]);

  const frameProcessor = useFrameProcessor((frame) => {
    'worklet';
    runAsync(frame, () => {
      'worklet';
      console.log(JSON.parse(recogniseFrame(frame)));
    });
  }, []);

  if (device == null)
    return (
      <View style={styles.container}>
        <Text>No device</Text>
      </View>
    );
  return (
    <Camera
      style={StyleSheet.absoluteFill}
      device={device}
      isActive={true}
      frameProcessor={frameProcessor}
    />
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
  },
  box: {
    width: 60,
    height: 60,
    marginVertical: 20,
  },
});
