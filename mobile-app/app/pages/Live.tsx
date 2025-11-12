import PageContainer from "../components/PageContainer";
import PageHeader from "../components/PageHeader";
import SPO2 from "../components/graphs/SPO2";
import HeartRate from "../components/graphs/HeartRate";

const Live = () => {
  return (
    <PageContainer>
      <PageHeader name="Live" />
      <SPO2 />
      <HeartRate />
    </PageContainer>
  );
};

export default Live;
