import type { AlprRegionOfInterest } from '../types/global';
import { installPlugin } from '../plugin';

type queueMethod = () => void;

export class ALPRService {
  private isInitialized: boolean = false;
  private methodQueue: queueMethod[] = [];

  constructor() {
    installPlugin();

    // Bind context for overloaded methods
    this.recognise = this.recognise.bind(this);
  }

  private queueOrExectute = (method: queueMethod) => {
    if (this.isInitialized) {
      method();
    } else {
      this.methodQueue.push(method);
    }
  };

  private queueOrExecuteAsync = <T>(method: () => T): Promise<T> => {
    return new Promise((resolve) => {
      if (this.isInitialized) {
        resolve(method());
      } else {
        this.methodQueue.push(() => resolve(method()));
      }
    });
  };

  private runMethodQueue = () => {
    this.methodQueue.forEach((method) => {
      method();
    });
    this.isInitialized = true;
  };

  // Initialize OpenALPR with country
  initialize = (country: string, topN?: number, region?: string) => {
    if (global.initializeANPR) {
      global.initializeANPR(country, topN, region);
      this.runMethodQueue();
    } else {
      throw new Error('Plugin not installed');
    }
  };

  // Set the number of top N results to consider
  setTopN = (topN: number) => {
    this.queueOrExectute(() => {
      if (global.setTopN) {
        global.setTopN(topN);
      } else {
        throw new Error('OpenALPR is not initialized');
      }
    });
  };

  // Set the country for recognition
  setCountry = (country: string) => {
    this.queueOrExectute(() => {
      if (global.setCountry) {
        global.setCountry(country);
      } else {
        throw new Error('OpenALPR is not initialized');
      }
    });
  };

  // Set the prewarp configuration
  setPrewarp = (prewarpConfig: string) => {
    this.queueOrExectute(() => {
      if (global.setPrewarp) {
        global.setPrewarp(prewarpConfig);
      } else {
        throw new Error('OpenALPR is not initialized');
      }
    });
  };

  // Set the detection mask with pixel data
  setMask = (
    pixelData: Uint8Array,
    bytesPerPixel: number,
    imgWidth: number,
    imgHeight: number
  ) => {
    this.queueOrExectute(() => {
      if (global.setMask) {
        global.setMask(pixelData, bytesPerPixel, imgWidth, imgHeight);
      } else {
        throw new Error('OpenALPR is not initialized');
      }
    });
  };

  // Set the detection region
  setDetectRegion = (detectRegion: boolean) => {
    this.queueOrExectute(() => {
      if (global.setDetectRegion) {
        global.setDetectRegion(detectRegion);
      } else {
        throw new Error('OpenALPR is not initialized');
      }
    });
  };

  // Set the default region for recognition
  setDefaultRegion = (region: string) => {
    this.queueOrExectute(() => {
      if (global.setDefaultRegion) {
        global.setDefaultRegion(region);
      } else {
        throw new Error('OpenALPR is not initialized');
      }
    });
  };

  // Keeping recognise methods unchanged
  async recognise(filePath: string): Promise<string>;
  async recognise(imgBytes: Uint8Array): Promise<string>;
  async recognise(
    imageBytes: Uint8Array,
    regionsOfInterest: AlprRegionOfInterest[]
  ): Promise<string>;
  async recognise(
    pixelData: Uint8Array,
    imgWidth: number,
    imgHeight: number,
    regionsOfInterest: AlprRegionOfInterest[]
  ): Promise<string>;
  async recognise(
    arg1: string | Uint8Array,
    arg2?: AlprRegionOfInterest[] | number,
    arg3?: number,
    arg4?: AlprRegionOfInterest[]
  ): Promise<string> {
    return this.queueOrExecuteAsync(() => {
      if (!global.recognise) {
        throw new Error('OpenALPR is not initialized');
      }

      // Recognise from file path
      if (typeof arg1 === 'string') {
        if (global.recognise) {
          return global.recognise(arg1);
        }
      }

      // Recognise from image bytes (simple byte array)
      if (arg1 instanceof Uint8Array && typeof arg2 === 'undefined') {
        if (global.recognise) {
          return global.recognise(arg1);
        }
      }

      // Recognise from image bytes with regions of interest
      if (arg1 instanceof Uint8Array && Array.isArray(arg2)) {
        if (global.recognise) {
          return global.recognise(arg1, arg2 as AlprRegionOfInterest[]);
        }
      }

      // Recognise from raw pixel data
      if (
        arg1 instanceof Uint8Array &&
        typeof arg2 === 'number' &&
        typeof arg3 === 'number' &&
        Array.isArray(arg4)
      ) {
        if (global.recognise) {
          return global.recognise(arg1, arg2, arg3, arg4);
        }
      }

      throw new Error('Invalid arguments passed to recognise');
    });
  }

  get recogniseFrame(): (frame: any) => string {
    if (global.recogniseFrame) {
      return global.recogniseFrame;
    } else {
      throw new Error('OpenALPR is not initialized');
    }
  }
}
