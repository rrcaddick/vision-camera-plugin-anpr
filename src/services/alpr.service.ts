import type { AlprRegionOfInterest } from '../types/global';
import { installPlugin } from '../plugin';

export class ALPRService {
  constructor() {
    installPlugin();
  }

  // Initialize OpenALPR with country
  initialize(country: string) {
    installPlugin();
    if (global.initializeANPR) {
      global.initializeANPR(country);
    } else {
      throw new Error('OpenALPR is not available');
    }
  }

  // Set the country for recognition
  setCountry(country: string) {
    if (global.setCountry) {
      global.setCountry(country);
    } else {
      throw new Error('OpenALPR is not initialized');
    }
  }

  // Set the prewarp configuration
  setPrewarp(prewarpConfig: string) {
    if (global.setPrewarp) {
      global.setPrewarp(prewarpConfig);
    } else {
      throw new Error('OpenALPR is not initialized');
    }
  }

  // Set the detection mask with pixel data
  setMask(
    pixelData: Uint8Array,
    bytesPerPixel: number,
    imgWidth: number,
    imgHeight: number
  ) {
    if (global.setMask) {
      global.setMask(pixelData, bytesPerPixel, imgWidth, imgHeight);
    } else {
      throw new Error('OpenALPR is not initialized');
    }
  }

  // Set the detection region
  setDetectRegion(detectRegion: boolean) {
    if (global.setDetectRegion) {
      global.setDetectRegion(detectRegion);
    } else {
      throw new Error('OpenALPR is not initialized');
    }
  }

  // Set the number of top N results to consider
  setTopN(topN: number) {
    if (global.setTopN) {
      global.setTopN(topN);
    } else {
      throw new Error('OpenALPR is not initialized');
    }
  }

  // Set the default region for recognition
  setDefaultRegion(region: string) {
    if (global.setDefaultRegion) {
      global.setDefaultRegion(region);
    } else {
      throw new Error('OpenALPR is not initialized');
    }
  }

  recognise(filePath: string): string;
  recognise(imgBytes: Uint8Array): string;
  recognise(
    imageBytes: Uint8Array,
    regionsOfInterest: AlprRegionOfInterest[]
  ): string;
  recognise(
    pixelData: Uint8Array,
    imgWidth: number,
    imgHeight: number,
    regionsOfInterest: AlprRegionOfInterest[]
  ): string;
  recognise(
    arg1: string | Uint8Array,
    arg2?: AlprRegionOfInterest[] | number,
    arg3?: number,
    arg4?: AlprRegionOfInterest[]
  ): string {
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
  }

  get recogniseFrame(): (frame: any) => string {
    if (global.recogniseFrame) {
      return global.recogniseFrame;
    } else {
      throw new Error('OpenALPR is not initialized');
    }
  }
}
