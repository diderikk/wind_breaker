import http from 'k6/http';
import { sleep } from 'k6';

export const options = {
  stages: [
    { duration: '20m', target: 500 }, // just slowly ramp-up to a HUGE load
  ],
};

export default function() {
  const url =  __ENV.API_ENDPOINT || "http://localhost:8080"

  const values = ["", "projects", "posts"];
  const randomIndex = Math.floor(Math.random() * 3); // 0, 1, or 2
  const assignedString = values[randomIndex];

  const projectIds = ["066691ec-2b6c-4799-b802-af92c989e84c", "55ba6e88-4d8f-4713-ae15-83d9d19e81f2", 
    "7730319e-38ec-4547-87aa-464498af92a7", "af583797-e988-4f2f-b7dd-b598e47c8ec1", "c12b5210-afc7-404a-9d66-a2245408456e", 
    "daa57ed2-5ee4-420b-9e09-6a0989488373"];

  const postIds = ["15642b10-1759-44c1-bf90-be91720d699d", "3f9e8f5f-263f-48d1-897b-2db50ea64561", "6c92de12-a2b2-4e14-bd3b-372f8a51a8aa", 
    "7d123de4-fb70-4ba0-9cca-f69c84c7a216", "b86ff89d-a28d-4b7a-8436-782dcc25dfe3", "ce163d0f-5c3c-4cca-9788-16871429b6b8", "dc034859-46bf-4d8c-8736-ad5b3c201021"];

  var fullUrl = `${url}/${assignedString}`;
  if(assignedString == "projects") {
    const i = Math.floor(Math.random() * projectIds.length);
    const id = projectIds[i];

    fullUrl = `${fullUrl}/${id}`;
  } else if (assignedString == "posts") {
    const i = Math.floor(Math.random() * postIds.length);
    const id = postIds[i];

    fullUrl = `${fullUrl}/${id}`;

  }

  sleep(1);
  http.get(fullUrl);
  sleep(1);
}
