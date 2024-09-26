import { View, StyleSheet, ActivityIndicator } from 'react-native';
import { useNavigation } from '@react-navigation/native';
import type { NativeStackNavigationProp } from '@react-navigation/native-stack';
import type { RootStackParamList } from '../types/navigation';
import { RoundedButton } from '../components/RoundedButton';
import {
  launchImageLibrary,
  type ImageLibraryOptions,
} from 'react-native-image-picker';
import { useALPR } from 'vision-camera-plugin-anpr';
import { useCallback, useState } from 'react';

type HomeScreenNavigationProp = NativeStackNavigationProp<
  RootStackParamList,
  'Home'
>;

export const HomeScreen = () => {
  const navigation = useNavigation<HomeScreenNavigationProp>();
  const { recognise } = useALPR();

  const [isLoading, setIsLoading] = useState(false);

  const base64ToUint8Array = useCallback((base64: string): Uint8Array => {
    const binaryString = atob(base64); // Decodes base64 to binary string
    const length = binaryString.length;
    const bytes = new Uint8Array(length);
    for (let i = 0; i < length; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }
    return bytes;
  }, []);

  const handleChooseFile = async () => {
    const options: ImageLibraryOptions = {
      mediaType: 'photo',
      includeBase64: true,
      maxHeight: 2000,
      maxWidth: 2000,
    };

    try {
      const result = await launchImageLibrary(options);
      const imageUri = result?.assets?.[0]?.uri;
      const originalPath = result?.assets?.[0]?.originalPath;
      const base64 = result?.assets?.[0]?.base64;

      if (imageUri && originalPath && base64) {
        setIsLoading(true);

        const imageBytes = base64ToUint8Array(base64);

        // Call the recognise function with the Uint8Array
        const plateResults = await recognise(imageBytes.buffer);

        navigation.navigate('ChooseFile', {
          imagePath: imageUri,
          results: plateResults,
        });
      }
    } catch (error) {
      console.error('Error in image selection or recognition:', error);
      // Handle error (e.g., show an alert)
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <View style={styles.container}>
      {isLoading ? (
        <ActivityIndicator size="large" color="#0000ff" />
      ) : (
        <>
          <RoundedButton
            title="Real Time Scan"
            onPress={() => navigation.navigate('RealTimeScan')}
          />
          <RoundedButton
            title="Real Time Recognise"
            onPress={() => navigation.navigate('RealTimeRecognise')}
          />
          <RoundedButton title="Choose File" onPress={handleChooseFile} />
        </>
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    gap: 50,
    padding: 50,
  },
});
