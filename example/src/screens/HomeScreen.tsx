import { View, StyleSheet } from 'react-native';
import { useNavigation } from '@react-navigation/native';
import type { NativeStackNavigationProp } from '@react-navigation/native-stack';
import type { RootStackParamList } from '../types/navigation';
import { RoundedButton } from '../components/RoundedButton';

type HomeScreenNavigationProp = NativeStackNavigationProp<
  RootStackParamList,
  'Home'
>;

export const HomeScreen = () => {
  const navigation = useNavigation<HomeScreenNavigationProp>();

  return (
    <View style={styles.container}>
      <RoundedButton
        title="Real Time Scan"
        onPress={() => navigation.navigate('RealTimeScan')}
      />
      <RoundedButton
        title="Real Time Recognise"
        onPress={() => navigation.navigate('RealTimeRecognise')}
      />
      <RoundedButton
        title="Choose File"
        onPress={() => navigation.navigate('ChooseFile')}
      />
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
