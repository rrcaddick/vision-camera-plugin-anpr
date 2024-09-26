import { PermissionsAndroid, Platform, Alert } from 'react-native';

const requestPermissions = async (): Promise<void> => {
  try {
    if (Platform.OS === 'android') {
      const permissionsToRequest = [
        'android.permission.CAMERA',
        'android.permission.WRITE_EXTERNAL_STORAGE',
        'android.permission.READ_EXTERNAL_STORAGE',
      ] as const;

      const permissions = permissionsToRequest.filter((permission) =>
        Object.values(PermissionsAndroid.PERMISSIONS).includes(permission)
      );

      const granted = await PermissionsAndroid.requestMultiple(permissions);

      const allGranted = Object.values(granted).every(
        (status) => status === PermissionsAndroid.RESULTS.GRANTED
      );

      if (allGranted) {
        console.log('All requested permissions granted');
      } else {
        console.log('Some permissions denied');
        Alert.alert(
          'Permissions Denied',
          'Some necessary permissions were denied. Please grant all required permissions to continue.'
        );
      }
    }
  } catch (err) {
    console.warn('Permission request error:', err);
  }
};

export default requestPermissions;
