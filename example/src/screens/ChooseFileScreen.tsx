import React from 'react';
import { View, Image, Text, StyleSheet } from 'react-native';
import { useNavigation, type RouteProp } from '@react-navigation/native';
import type { RootStackParamList } from '../types/navigation';
import { RoundedButton } from '../components/RoundedButton';
import type { NativeStackNavigationProp } from '@react-navigation/native-stack';

type ChooseFileScreenRouteProp = RouteProp<RootStackParamList, 'ChooseFile'>;
type ChooseFileScreenNavigationProp = NativeStackNavigationProp<
  RootStackParamList,
  'ChooseFile'
>;

type Props = {
  route: ChooseFileScreenRouteProp;
};

export const ChooseFileScreen: React.FC<Props> = ({ route }) => {
  const navigation = useNavigation<ChooseFileScreenNavigationProp>();
  const { imagePath, results } = route.params;

  return (
    <View style={styles.container}>
      <Image
        source={{ uri: imagePath }}
        style={styles.image}
        resizeMode="cover"
      />
      <View style={styles.resultsContainer}>
        <Text style={styles.resultText}>{results}</Text>
      </View>
      <View style={styles.buttonContainer}>
        <RoundedButton
          title="Back Home"
          onPress={() => navigation.navigate('Home')}
        />
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    flexDirection: 'column',
  },
  image: {
    flex: 1, // This will make the image take up half the screen
  },
  resultsContainer: {
    flex: 1, // This will make the results take up the other half
    padding: 10,
  },
  resultText: {
    color: 'black',
  },
  buttonContainer: {
    padding: 10,
  },
});
