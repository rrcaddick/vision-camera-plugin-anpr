import { useContext } from 'react';
import { ALPRContext } from '../context/alpr.context';

export const useALPR = () => {
  const context = useContext(ALPRContext);
  if (!context) {
    throw new Error('useALPR must be used within an ALPRProvider');
  }

  return context.alprService;
};
