import { View, Button, StyleSheet } from 'react-native';
import { useNavigation } from '@react-navigation/native';
import type { NativeStackNavigationProp } from '@react-navigation/native-stack';
import type { RootStackParamList } from '../types/navigation';

type ChooseFileScreenNavigationProp = NativeStackNavigationProp<
  RootStackParamList,
  'RealTimeRecognise'
>;

export const ChooseFileScreen = () => {
  const navigation = useNavigation<ChooseFileScreenNavigationProp>();

  return (
    <View style={styles.container}>
      <Button title="Home" onPress={() => navigation.navigate('Home')} />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    gap: 20,
  },
});
