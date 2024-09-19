import { createContext, useEffect } from 'react';
import type { FC, ReactNode } from 'react';
import { ALPRService } from '../services/alpr.service';

interface ALPRContextType {
  alprService: ALPRService;
}

interface ALPRProviderProps {
  children: ReactNode;
  config: {
    country: string;
    topN: number;
    region?: string;
  };
}

const alprService = new ALPRService();

export const ALPRContext = createContext<ALPRContextType | null>(null);

export const ALPRProvider: FC<ALPRProviderProps> = ({
  children,
  config: { country, topN, region },
}) => {
  useEffect(() => {
    alprService.initialize(country, topN, region);

    // TD: return cleanup function
    // return () => { ... }
  }, []);

  return (
    <ALPRContext.Provider value={{ alprService }}>
      {children}
    </ALPRContext.Provider>
  );
};
