import { Frame } from 'react-native-vision-camera';

export interface AlprRegionOfInterest {
  x: number;
  y: number;
  width: number;
  height: number;
}

// global.d.ts
declare global {
  function initializeANPR(
    country: string,
    topN?: number,
    region?: string
  ): void;
  function setCountry(country: string): void;
  function setPrewarp(prewarpConfig: string): void;
  function setMask(
    pixelData: Uint8Array,
    bytesPerPixel: number,
    imgWidth: number,
    imgHeight: number
  ): void;
  function setDetectRegion(detectRegion: Boolean): void;
  function setTopN(topN: number): void;
  function setDefaultRegion(region: string): void;
  function recognise(filePath: string): string;
  function recognise(imgBytes: ArrayBuffer): string;
  function recognise(
    imageBytes: ArrayBuffer,
    regionsOfInterest: AlprRegionOfInterest[]
  ): string;
  function recognise(
    pixelData: ArrayBuffer,
    imgWidth: number,
    imgHeight: number,
    regionsOfInterest: AlprRegionOfInterest[]
  ): string;
  function recogniseFrame(frame: Frame): string;
  __turboModuleProxy;
}

export {};
