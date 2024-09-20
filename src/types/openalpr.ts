interface AlprCoordinate {
  x: number;
  y: number;
}

interface AlprChar {
  corners: [AlprCoordinate, AlprCoordinate, AlprCoordinate, AlprCoordinate];
  confidence: number;
  character: string;
}

interface AlprPlate {
  characters: string;
  overall_confidence: number;
  character_details: AlprChar[];
  matches_template: boolean;
}

interface AlprPlateResult {
  requested_topn: number;
  country: string;
  bestPlate: AlprPlate;
  topNPlates: AlprPlate[];
  processing_time_ms: number;
  plate_points: [
    AlprCoordinate,
    AlprCoordinate,
    AlprCoordinate,
    AlprCoordinate,
  ];
  plate_index: number;
  regionConfidence: number;
  region: string;
}

interface AlprResults {
  epoch_time: number;
  frame_number: number;
  img_width: number;
  img_height: number;
  total_processing_time_ms: number;
  plates: AlprPlateResult[];
  regionsOfInterest: AlprRegionOfInterest[];
}

interface AlprRegionOfInterest {
  x: number;
  y: number;
  width: number;
  height: number;
}

function parseAlprResults(jsonString: string): AlprResults {
  try {
    const results: AlprResults = JSON.parse(jsonString);

    // Validate the parsed object (you might want to add more thorough validation)
    if (
      typeof results.epoch_time !== 'number' ||
      !Array.isArray(results.plates)
    ) {
      throw new Error('Invalid ALPR results format');
    }

    return results;
  } catch (error) {
    console.error('Error parsing ALPR results:', error);
    throw error;
  }
}

// Usage example:
try {
  const jsonString =
    '{"epoch_time": 1234567890, "frame_number": 1, "img_width": 800, "img_height": 600, "total_processing_time_ms": 100.5, "plates": [...], "regionsOfInterest": [...]}';
  const alprResults = parseAlprResults(jsonString);
  console.log('Parsed ALPR Results:', alprResults);

  // You can now access the structured data, for example:
  console.log('Number of plates detected:', alprResults.plates.length);
  console.log(
    'Best plate for first result:',
    alprResults.plates[0]?.bestPlate.characters
  );
} catch (error) {
  console.error('Failed to parse ALPR results:', error);
}
