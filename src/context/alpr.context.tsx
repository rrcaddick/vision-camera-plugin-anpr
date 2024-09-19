import { createContext, useContext, useEffect } from 'react';
import type { FC, ReactNode } from 'react';
import { ALPRService } from '../services/alpr.service';

interface ALPRContextType {
  alprService: ALPRService;
}

interface ALPRProviderProps {
  children: ReactNode;
  country: string;
  topN: number;
  region?: string;
}

const ALPRContext = createContext<ALPRContextType | null>(null);

const alprService = new ALPRService();

export const ALPRProvider: FC<ALPRProviderProps> = ({ children, country }) => {
  useEffect(() => {
    alprService.initialize(country);
    // TD: return cleanup function
    // return () => { ... }
  }, []);

  return (
    <ALPRContext.Provider value={{ alprService }}>
      {children}
    </ALPRContext.Provider>
  );
};

export const useALPR = () => {
  const context = useContext(ALPRContext);
  if (!context) {
    throw new Error('useALPR must be used within an ALPRProvider');
  }
  return {
    recogniseFrame: context.alprService.recogniseFrame,
  };
};
